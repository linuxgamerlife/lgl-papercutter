#pragma once

#include "core/imagejob.h"

#include <functional>
#include <string>
#include <vector>

namespace papercutter {

struct ProcessingRequest {
    ImageJob job;
    std::string destinationPath;
    bool overwriteDestination{false};
};

struct ProcessingResult {
    bool succeeded{false};
    std::string sourcePath;
    std::string outputPath;
    std::string failureStage;
    std::string errorMessage;
};

// Renders jobs with ImageMagick and writes attributes with Exiv2. Both are started directly
// (argv, never a shell) as Gio::Subprocess and awaited on the thread-default main context,
// so the *Async functions never block. Completion callbacks may run before the call
// returns when validation fails early. The blocking variants iterate a private main
// context until the asynchronous work finishes; they are meant for tests and tools.
class ImageProcessor final {
public:
    using ProcessCallback = std::function<void(const ProcessingResult &)>;
    using SizeCallback = std::function<void(long long)>;
    using MetadataCallback = std::function<void(const ImageMetadata &)>;

    static bool isBackendAvailable();
    static std::string backendExecutable();
    static std::string metadataExecutable();

    // ImageMagick arguments for a job; the output path is the last argument.
    static std::vector<std::string> renderArguments(const ImageJob &job,
                                                    const std::string &outputPath);

    static void processAsync(const ProcessingRequest &request, ProcessCallback done);
    static void estimateOutputSizeAsync(const ImageJob &job, const std::string &suffix,
                                        SizeCallback done);
    static void readMetadataAsync(const std::string &path, MetadataCallback done);

    static ProcessingResult process(const ProcessingRequest &request);
    static long long estimateOutputSize(const ImageJob &job, const std::string &suffix);
    static ImageMetadata readMetadata(const std::string &path);
};

} // namespace papercutter
