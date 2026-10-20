#include "core/processor.h"

#include "core/imageinfo.h"

#include <giomm/file.h>
#include <giomm/subprocess.h>
#include <glib/gstdio.h>
#include <glibmm/fileutils.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <glibmm/bytes.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <unistd.h>

namespace papercutter {

namespace {

struct MetadataField {
    const char *key;
    std::string ImageMetadata::*member;
};

const MetadataField metadataFields[] = {
    {"Exif.Image.ImageDescription", &ImageMetadata::description},
    {"Exif.Image.Artist", &ImageMetadata::artist},
    {"Exif.Image.Copyright", &ImageMetadata::copyright},
    {"Exif.Photo.UserComment", &ImageMetadata::comment},
};

constexpr unsigned int renderTimeoutMs = 300000;
constexpr unsigned int metadataTimeoutMs = 30000;

std::string trimmed(const std::string &text)
{
    const char *whitespace = " \t\r\n\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string::npos)
        return {};
    const auto last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

bool startsWith(const std::string &text, const std::string &prefix)
{
    return text.compare(0, prefix.size(), prefix) == 0;
}

std::string bytesToString(const Glib::RefPtr<Glib::Bytes> &bytes)
{
    if (!bytes)
        return {};
    gsize size = 0;
    const auto *data = static_cast<const char *>(bytes->get_data(size));
    return data ? std::string(data, size) : std::string();
}

// Normalised absolute path (".", ".." and duplicate separators removed), like
// QFileInfo::absoluteFilePath() for comparisons.
std::string absolutePath(const std::string &path)
{
    return Gio::File::create_for_path(path)->get_path();
}

struct CommandResult {
    bool started{false};
    bool timedOut{false};
    bool succeeded{false};
    std::string output;
    std::string errors;
};

// Runs argv asynchronously on the thread-default main context; force-exits the child after
// timeoutMs.
void runCommand(const std::vector<std::string> &argv, const unsigned int timeoutMs,
                std::function<void(const CommandResult &)> done)
{
    Glib::RefPtr<Gio::Subprocess> process;
    try {
        process = Gio::Subprocess::create(
            argv, Gio::Subprocess::Flags::STDOUT_PIPE | Gio::Subprocess::Flags::STDERR_PIPE);
    } catch (const Glib::Error &) {
        done({});
        return;
    }

    auto timedOut = std::make_shared<bool>(false);
    auto timeout = Glib::TimeoutSource::create(timeoutMs);
    timeout->connect([process, timedOut] {
        *timedOut = true;
        process->force_exit();
        return false;
    });
    timeout->attach(Glib::MainContext::get_thread_default());

    process->communicate_async(
        {}, [process, timedOut, timeout, done = std::move(done)](
                Glib::RefPtr<Gio::AsyncResult> &asyncResult) {
            timeout->destroy();
            CommandResult result;
            result.started = true;
            try {
                const auto [output, errors] = process->communicate_finish(asyncResult);
                result.output = bytesToString(output);
                result.errors = bytesToString(errors);
                result.succeeded = !*timedOut && process->get_successful();
            } catch (const Glib::Error &error) {
                result.errors = error.what();
            }
            result.timedOut = *timedOut;
            done(result);
        });
}

std::vector<std::string> metadataWriteArguments(const std::string &executable,
                                                const std::string &path,
                                                const ImageMetadata &metadata)
{
    std::vector<std::string> arguments{executable};
    for (const MetadataField &field : metadataFields) {
        const std::string key = field.key;
        const std::string &value = metadata.*(field.member);
        std::string command;
        if (value.empty())
            command = "del " + key;
        else if (key == "Exif.Photo.UserComment")
            command = "set " + key + " charset=Unicode " + value;
        else
            command = "set " + key + " Ascii " + value;
        arguments.push_back("-M");
        arguments.push_back(command);
    }
    arguments.push_back(path);
    return arguments;
}

std::string metadataValue(const std::string &output, const std::string &key)
{
    const std::string prefix = key + ' ';
    std::size_t start = 0;
    while (start <= output.size()) {
        auto end = output.find('\n', start);
        if (end == std::string::npos)
            end = output.size();
        const std::string line = output.substr(start, end - start);
        start = end + 1;
        if (!startsWith(line, key))
            continue;
        std::string value = line.size() > prefix.size()
            ? trimmed(line.substr(prefix.size())) : std::string();
        if (key == "Exif.Photo.UserComment" && startsWith(value, "charset=Unicode "))
            value.erase(0, 16);
        return value;
    }
    return {};
}

// A temporary output that is removed unless it was committed.
struct TemporaryOutput {
    std::string path;
    bool committed{false};

    ~TemporaryOutput()
    {
        if (!committed && !path.empty())
            g_remove(path.c_str());
    }
};

ProcessingResult failure(const ImageJob &job, const std::string &outputPath,
                         const std::string &stage, const std::string &message)
{
    return {false, job.sourcePath, outputPath, stage, message};
}

void commitOutput(const ProcessingRequest &request, const std::string &outputPath,
                  const std::shared_ptr<TemporaryOutput> &temporary,
                  const ImageProcessor::ProcessCallback &done)
{
    const ImageJob &job = request.job;
    const Size target = job.composition.targetSize;
    const auto rendered = readImageDimensions(temporary->path);
    if (!rendered || *rendered != target) {
        done(failure(job, outputPath, "output-validation",
                     "The rendered image has incorrect dimensions."));
        return;
    }

    std::error_code error;
    const auto permissions = std::filesystem::status(job.sourcePath, error).permissions();
    if (!error)
        std::filesystem::permissions(temporary->path, permissions, error);
    error.clear();
    std::filesystem::rename(temporary->path, outputPath, error);
    if (error) {
        done(failure(job, outputPath, "commit",
                     "The processed image could not be committed at " + outputPath + ": "
                         + error.message()));
        return;
    }
    temporary->committed = true;

    const auto committed = readImageDimensions(outputPath);
    if (!committed || *committed != target) {
        done(failure(job, outputPath, "commit-validation",
                     "The committed image at " + outputPath + " could not be verified at "
                         + std::to_string(target.width) + " × "
                         + std::to_string(target.height) + "."));
        return;
    }
    ProcessingResult result;
    result.succeeded = true;
    result.sourcePath = job.sourcePath;
    result.outputPath = outputPath;
    done(result);
}

// Runs an asynchronous operation to completion on a private main context.
template <typename T, typename Start>
T waitFor(Start start)
{
    auto context = Glib::MainContext::create();
    context->push_thread_default();
    std::optional<T> result;
    start([&result](const T &value) { result = value; });
    while (!result)
        context->iteration(true);
    context->pop_thread_default();
    return *result;
}

} // namespace

bool ImageProcessor::isBackendAvailable()
{
    return !backendExecutable().empty();
}

std::string ImageProcessor::backendExecutable()
{
    const std::string packagedExecutable = Glib::getenv("PAPERCUTTER_IMAGEMAGICK");
    if (!packagedExecutable.empty()
        && Glib::file_test(packagedExecutable, Glib::FileTest::IS_EXECUTABLE)
        && !Glib::file_test(packagedExecutable, Glib::FileTest::IS_DIR))
        return packagedExecutable;
    const std::string modernExecutable = Glib::find_program_in_path("magick");
    if (!modernExecutable.empty())
        return modernExecutable;
    return Glib::find_program_in_path("convert");
}

std::string ImageProcessor::metadataExecutable()
{
    return Glib::find_program_in_path("exiv2");
}

std::vector<std::string> ImageProcessor::renderArguments(const ImageJob &job,
                                                         const std::string &outputPath)
{
    const Size target = job.composition.targetSize;
    const double scale = job.composition.effectiveScaleFor(job.sourceSize);
    const int scaledWidth = std::max(1, static_cast<int>(std::lround(job.sourceSize.width * scale)));
    const int scaledHeight = std::max(1, static_cast<int>(std::lround(job.sourceSize.height * scale)));
    const int travelX = std::max(0, (scaledWidth - target.width) / 2);
    const int travelY = std::max(0, (scaledHeight - target.height) / 2);
    const int imageX = (target.width - scaledWidth) / 2
        + static_cast<int>(std::lround(job.composition.normalizedOffset.x * travelX));
    const int imageY = (target.height - scaledHeight) / 2
        + static_cast<int>(std::lround(job.composition.normalizedOffset.y * travelY));
    const std::string geometry = std::to_string(scaledWidth) + 'x' + std::to_string(scaledHeight)
        + (imageX >= 0 ? "+" : "") + std::to_string(imageX)
        + (imageY >= 0 ? "+" : "") + std::to_string(imageY);
    const std::string canvasSize = std::to_string(target.width) + 'x'
        + std::to_string(target.height);
    const std::string scaledSize = std::to_string(scaledWidth) + 'x'
        + std::to_string(scaledHeight) + '!';

    std::vector<std::string> arguments{
        "-size", canvasSize, "xc:black",
        "(", job.sourcePath, "-auto-orient", "-filter", "Lanczos", "-resize", scaledSize, ")",
        "-geometry", geometry, "-composite"};

    const std::string suffix = fileSuffix(outputPath);
    const std::string quality = std::to_string(job.outputQuality);
    if (suffix == "jpg" || suffix == "jpeg") {
        arguments.insert(arguments.end(), {"-sampling-factor", "4:4:4", "-quality", quality});
    } else if (suffix == "webp") {
        arguments.insert(arguments.end(), {"-quality", quality});
    } else if (suffix == "png") {
        if (isIndexedPng(job.sourcePath))
            arguments.insert(arguments.end(), {"-colors", "256", "-define", "png:color-type=3"});
        arguments.insert(arguments.end(), {"-define", "png:compression-level=9"});
    }
    arguments.push_back(outputPath);
    return arguments;
}

void ImageProcessor::processAsync(const ProcessingRequest &request, ProcessCallback done)
{
    const ImageJob &job = request.job;
    if (!job.composition.isValid()) {
        done(failure(job, {}, "validation", "The composition is invalid."));
        return;
    }
    if (job.outputQuality < 1 || job.outputQuality > 100) {
        done(failure(job, {}, "validation", "Output quality must be between 1 and 100."));
        return;
    }
    if (!Glib::file_test(job.sourcePath, Glib::FileTest::EXISTS)) {
        done(failure(job, {}, "validation", "The source image does not exist."));
        return;
    }
    if (!isBackendAvailable()) {
        done(failure(job, {}, "backend",
                     "ImageMagick is not installed; neither magick nor convert is in PATH."));
        return;
    }
    if (request.destinationPath.empty()) {
        done(failure(job, {}, "validation", "No Save As destination was selected."));
        return;
    }
    const std::string outputPath = absolutePath(request.destinationPath);
    if (outputPath == absolutePath(job.sourcePath)) {
        done(failure(job, outputPath, "validation", "Save As cannot replace the original image."));
        return;
    }
    if (Glib::file_test(outputPath, Glib::FileTest::EXISTS) && !request.overwriteDestination) {
        done(failure(job, outputPath, "collision", "The destination already exists."));
        return;
    }
    const std::string outputDirectory = Glib::path_get_dirname(outputPath);
    if (g_mkdir_with_parents(outputDirectory.c_str(), 0755) != 0) {
        done(failure(job, outputPath, "destination",
                     "The destination folder could not be created."));
        return;
    }
    if (!isSupportedSuffix(fileSuffix(outputPath))) {
        done(failure(job, outputPath, "validation",
                     "The destination must use JPEG, PNG, or WebP."));
        return;
    }

    std::string temporaryTemplate = Glib::build_filename(
        outputDirectory, ".lgl-papercutter-XXXXXX." + rawFileSuffix(outputPath));
    const int descriptor = g_mkstemp(temporaryTemplate.data());
    if (descriptor < 0) {
        done(failure(job, outputPath, "temporary-file",
                     "A temporary output file could not be created."));
        return;
    }
    close(descriptor);
    auto temporary = std::make_shared<TemporaryOutput>();
    temporary->path = temporaryTemplate;

    std::vector<std::string> arguments = renderArguments(job, temporary->path);
    arguments.insert(arguments.begin(), backendExecutable());
    runCommand(arguments, renderTimeoutMs,
               [request, outputPath, temporary, done](const CommandResult &render) {
        const ImageJob &job = request.job;
        if (!render.started) {
            done(failure(job, outputPath, "render", "ImageMagick could not be started."));
            return;
        }
        if (render.timedOut) {
            done(failure(job, outputPath, "render", "ImageMagick timed out."));
            return;
        }
        if (!render.succeeded) {
            const std::string diagnostic = trimmed(render.errors);
            done(failure(job, outputPath, "render",
                         diagnostic.empty() ? "ImageMagick failed." : diagnostic));
            return;
        }

        if (!job.metadataEdited && job.metadata.isEmpty()) {
            commitOutput(request, outputPath, temporary, done);
            return;
        }
        const std::string executable = metadataExecutable();
        if (executable.empty()) {
            done(failure(job, outputPath, "metadata",
                         "Exiv2 is required to save image attributes."));
            return;
        }
        runCommand(metadataWriteArguments(executable, temporary->path, job.metadata),
                   metadataTimeoutMs,
                   [request, outputPath, temporary, done](const CommandResult &metadata) {
            const ImageJob &job = request.job;
            if (!metadata.started) {
                done(failure(job, outputPath, "metadata", "Exiv2 could not be started."));
                return;
            }
            if (metadata.timedOut) {
                done(failure(job, outputPath, "metadata",
                             "Exiv2 timed out while saving image attributes."));
                return;
            }
            if (!metadata.succeeded) {
                const std::string diagnostic = trimmed(metadata.errors);
                done(failure(job, outputPath, "metadata",
                             diagnostic.empty() ? "Exiv2 could not save the image attributes."
                                                : diagnostic));
                return;
            }
            commitOutput(request, outputPath, temporary, done);
        });
    });
}

void ImageProcessor::estimateOutputSizeAsync(const ImageJob &job, const std::string &suffix,
                                             SizeCallback done)
{
    std::string normalizedSuffix = suffix;
    std::transform(normalizedSuffix.begin(), normalizedSuffix.end(), normalizedSuffix.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!isSupportedSuffix(normalizedSuffix)) {
        done(-1);
        return;
    }
    gchar *created = g_dir_make_tmp("lgl-papercutter-XXXXXX", nullptr);
    if (!created) {
        done(-1);
        return;
    }
    const std::string directory = created;
    g_free(created);
    const std::string outputPath = Glib::build_filename(directory, "estimate." + normalizedSuffix);
    processAsync({job, outputPath, false},
                 [directory, outputPath, done](const ProcessingResult &result) {
        long long size = -1;
        std::error_code error;
        if (result.succeeded) {
            const auto bytes = std::filesystem::file_size(outputPath, error);
            if (!error)
                size = static_cast<long long>(bytes);
        }
        std::filesystem::remove_all(directory, error);
        done(size);
    });
}

void ImageProcessor::readMetadataAsync(const std::string &path, MetadataCallback done)
{
    const std::string executable = metadataExecutable();
    if (executable.empty() || path.empty()) {
        done({});
        return;
    }
    std::vector<std::string> arguments{executable};
    for (const MetadataField &field : metadataFields) {
        arguments.push_back("-K");
        arguments.push_back(field.key);
    }
    arguments.push_back("-Pkt");
    arguments.push_back(path);
    runCommand(arguments, metadataTimeoutMs, [done](const CommandResult &result) {
        ImageMetadata metadata;
        if (result.succeeded) {
            for (const MetadataField &field : metadataFields)
                metadata.*(field.member) = metadataValue(result.output, field.key);
        }
        done(metadata);
    });
}

ProcessingResult ImageProcessor::process(const ProcessingRequest &request)
{
    return waitFor<ProcessingResult>([&request](auto finish) { processAsync(request, finish); });
}

long long ImageProcessor::estimateOutputSize(const ImageJob &job, const std::string &suffix)
{
    return waitFor<long long>([&](auto finish) { estimateOutputSizeAsync(job, suffix, finish); });
}

ImageMetadata ImageProcessor::readMetadata(const std::string &path)
{
    return waitFor<ImageMetadata>([&path](auto finish) { readMetadataAsync(path, finish); });
}

} // namespace papercutter
