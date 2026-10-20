#include "core/imageinfo.h"
#include "core/processor.h"

#include <giomm/init.h>
#include <giomm/subprocess.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace papercutter;

namespace {

struct TemporaryFolder {
    std::string path;

    TemporaryFolder()
    {
        gchar *created = g_dir_make_tmp("papercutter-processor-XXXXXX", nullptr);
        g_assert_nonnull(created);
        path = created;
        g_free(created);
    }
    ~TemporaryFolder()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::string file(const std::string &name) const { return path + "/" + name; }
};

// Creates a test image with ImageMagick (the same backend the processor needs).
void run(const std::vector<std::string> &arguments)
{
    auto process = Gio::Subprocess::create(arguments, Gio::Subprocess::Flags::STDERR_PIPE);
    process->wait();
    g_assert_true(process->get_successful());
}

void magick(std::vector<std::string> arguments)
{
    arguments.insert(arguments.begin(), ImageProcessor::backendExecutable());
    run(arguments);
}

Size dimensions(const std::string &path)
{
    const auto size = readImageDimensions(path);
    g_assert_true(size.has_value());
    return *size;
}

long long fileSize(const std::string &path)
{
    return static_cast<long long>(std::filesystem::file_size(path));
}

bool skipWithoutBackend()
{
    if (ImageProcessor::isBackendAvailable())
        return false;
    g_test_skip("ImageMagick is not available");
    return true;
}

void assertSucceeded(const ProcessingResult &result)
{
    if (!result.succeeded)
        g_test_message("%s: %s", result.failureStage.c_str(), result.errorMessage.c_str());
    g_assert_true(result.succeeded);
}

void packagedBackendOverridesHostDiscovery()
{
    TemporaryFolder folder;
    const std::string executablePath = folder.file("convert");
    std::ofstream(executablePath) << "#!/bin/sh\nexit 0\n";
    g_assert_cmpint(g_chmod(executablePath.c_str(), 0700), ==, 0);

    const char *previous = g_getenv("PAPERCUTTER_IMAGEMAGICK");
    const std::string previousValue = previous ? previous : "";
    g_setenv("PAPERCUTTER_IMAGEMAGICK", executablePath.c_str(), TRUE);
    const std::string selectedExecutable = ImageProcessor::backendExecutable();
    if (previous)
        g_setenv("PAPERCUTTER_IMAGEMAGICK", previousValue.c_str(), TRUE);
    else
        g_unsetenv("PAPERCUTTER_IMAGEMAGICK");

    g_assert_true(selectedExecutable == executablePath);
}

void fillsTargetWithoutGaps()
{
    if (skipWithoutBackend())
        return;
    TemporaryFolder folder;
    const std::string sourcePath = folder.file("portrait.png");
    const std::string destinationPath = folder.file("output.png");
    magick({"-size", "60x120", "xc:green", sourcePath});

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = {60, 120};
    job.composition.targetSize = {160, 90};
    job.composition.normalizedOffset = {0.0, -1.0};
    const auto result = ImageProcessor::process({job, destinationPath, false});
    assertSucceeded(result);
    g_assert_true(dimensions(destinationPath) == (Size{160, 90}));
    g_assert_true(dimensions(sourcePath) == (Size{60, 120}));
}

void writesEditedMetadataToExport()
{
    if (skipWithoutBackend())
        return;
    if (ImageProcessor::metadataExecutable().empty()) {
        g_test_skip("Exiv2 is not available");
        return;
    }
    TemporaryFolder folder;
    const std::string sourcePath = folder.file("source.jpg");
    const std::string destinationPath = folder.file("output.jpg");
    magick({"-size", "120x80", "xc:yellow", sourcePath});

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = {120, 80};
    job.composition.targetSize = {60, 40};
    job.metadata.description = "Mountain sunset";
    job.metadata.artist = "Test Artist";
    job.metadata.copyright = "Copyright 2026";
    job.metadata.comment = "Edited in Papercutter";
    job.metadataEdited = true;

    const auto result = ImageProcessor::process({job, destinationPath, false});
    assertSucceeded(result);
    const auto exported = ImageProcessor::readMetadata(destinationPath);
    g_assert_true(exported.description == job.metadata.description);
    g_assert_true(exported.artist == job.metadata.artist);
    g_assert_true(exported.copyright == job.metadata.copyright);
    g_assert_true(exported.comment == job.metadata.comment);
    g_assert_true(ImageProcessor::readMetadata(sourcePath).isEmpty());
}

void lowerJpegQualityReducesFileSize()
{
    if (skipWithoutBackend())
        return;
    TemporaryFolder folder;
    const std::string sourcePath = folder.file("source.png");
    const std::string highQualityPath = folder.file("high.jpg");
    const std::string lowQualityPath = folder.file("low.jpg");
    magick({"-seed", "2026", "-size", "512x512", "xc:", "+noise", "Random", sourcePath});

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = {512, 512};
    job.composition.targetSize = job.sourceSize;
    job.outputQuality = 95;
    assertSucceeded(ImageProcessor::process({job, highQualityPath, false}));

    job.outputQuality = 40;
    assertSucceeded(ImageProcessor::process({job, lowQualityPath, false}));
    g_assert_cmpint(fileSize(lowQualityPath), <, fileSize(highQualityPath));
    const long long estimatedSize = ImageProcessor::estimateOutputSize(job, "jpg");
    g_assert_cmpint(estimatedSize, >, 0);
    g_assert_cmpint(estimatedSize, ==, fileSize(lowQualityPath));
}

void keepsIndexedPngOutputsPaletteCompressed()
{
    if (skipWithoutBackend())
        return;
    TemporaryFolder folder;
    const std::string sourcePath = folder.file("indexed.png");
    const std::string destinationPath = folder.file("output.png");
    magick({"-seed", "7", "-size", "512x256", "xc:", "+noise", "Random", "-colors", "256",
            "PNG8:" + sourcePath});
    g_assert_true(isIndexedPng(sourcePath));

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = {512, 256};
    job.composition.targetSize = {800, 256};
    job.composition.zoom = 1.001 / job.composition.baseScaleFor(job.sourceSize);
    assertSucceeded(ImageProcessor::process({job, destinationPath, false}));

    g_assert_true(dimensions(destinationPath) == job.composition.targetSize);
    g_assert_true(isIndexedPng(destinationPath));
}

void savesACopyWithoutChangingTheOriginal()
{
    if (skipWithoutBackend())
        return;
    TemporaryFolder folder;
    const std::string sourcePath = folder.file("wallpaper.png");
    const std::string destinationPath = folder.file("exports/wallpaper.png");
    magick({"-size", "120x80", "xc:red", sourcePath});

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = {120, 80};
    job.composition.targetSize = {60, 100};
    assertSucceeded(ImageProcessor::process({job, destinationPath, false}));
    g_assert_true(dimensions(sourcePath) == (Size{120, 80}));
    g_assert_true(dimensions(destinationPath) == (Size{60, 100}));
    // No temporary output is left behind.
    int entries = 0;
    for ([[maybe_unused]] const auto &entry :
         std::filesystem::directory_iterator(folder.file("exports")))
        ++entries;
    g_assert_cmpint(entries, ==, 1);
}

void refusesToReplaceTheOriginal()
{
    if (skipWithoutBackend())
        return;
    TemporaryFolder folder;
    const std::string sourcePath = folder.file("original.png");
    magick({"-size", "120x80", "xc:blue", sourcePath});

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = {120, 80};
    job.composition.targetSize = {60, 40};
    const auto result = ImageProcessor::process({job, sourcePath, true});
    g_assert_false(result.succeeded);
    g_assert_true(result.failureStage == "validation");
    g_assert_true(dimensions(sourcePath) == (Size{120, 80}));
}

void refusesExistingDestinationWithoutOverwrite()
{
    if (skipWithoutBackend())
        return;
    TemporaryFolder folder;
    const std::string sourcePath = folder.file("source.png");
    const std::string destinationPath = folder.file("taken.png");
    magick({"-size", "120x80", "xc:blue", sourcePath});
    magick({"-size", "10x10", "xc:red", destinationPath});

    ImageJob job;
    job.sourcePath = sourcePath;
    job.sourceSize = {120, 80};
    job.composition.targetSize = {60, 40};
    const auto refused = ImageProcessor::process({job, destinationPath, false});
    g_assert_false(refused.succeeded);
    g_assert_true(refused.failureStage == "collision");
    assertSucceeded(ImageProcessor::process({job, destinationPath, true}));
    g_assert_true(dimensions(destinationPath) == (Size{60, 40}));
}

void readsOrientedJpegDimensions()
{
    if (skipWithoutBackend())
        return;
    TemporaryFolder folder;
    const std::string sourcePath = folder.file("rotated.jpg");
    const std::string webpPath = folder.file("lossless.webp");
    magick({"-size", "33x17", "xc:white", "-define", "webp:lossless=true", webpPath});
    g_assert_true(dimensions(webpPath) == (Size{33, 17}));
    magick({"-size", "120x80", "xc:white", sourcePath});
    g_assert_true(dimensions(sourcePath) == (Size{120, 80}));
    const std::string exiv2 = ImageProcessor::metadataExecutable();
    if (exiv2.empty()) {
        g_test_skip("Exiv2 is not available to tag the orientation");
        return;
    }
    run({exiv2, "-M", "set Exif.Image.Orientation Short 6", sourcePath});
    g_assert_true(dimensions(sourcePath) == (Size{80, 120}));
}

} // namespace

int main(int argc, char *argv[])
{
    Gio::init();
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/processor/packaged-backend-overrides-host-discovery",
                    packagedBackendOverridesHostDiscovery);
    g_test_add_func("/processor/fills-target-without-gaps", fillsTargetWithoutGaps);
    g_test_add_func("/processor/writes-edited-metadata-to-export", writesEditedMetadataToExport);
    g_test_add_func("/processor/lower-jpeg-quality-reduces-file-size",
                    lowerJpegQualityReducesFileSize);
    g_test_add_func("/processor/keeps-indexed-png-outputs-palette-compressed",
                    keepsIndexedPngOutputsPaletteCompressed);
    g_test_add_func("/processor/saves-a-copy-without-changing-the-original",
                    savesACopyWithoutChangingTheOriginal);
    g_test_add_func("/processor/refuses-to-replace-the-original", refusesToReplaceTheOriginal);
    g_test_add_func("/processor/refuses-existing-destination-without-overwrite",
                    refusesExistingDestinationWithoutOverwrite);
    g_test_add_func("/processor/reads-oriented-jpeg-dimensions", readsOrientedJpegDimensions);
    return g_test_run();
}
