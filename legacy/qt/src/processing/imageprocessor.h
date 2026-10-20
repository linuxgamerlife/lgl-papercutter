#pragma once

#include "core/imagejob.h"

#include <QString>

namespace papercutter {

struct ProcessingRequest {
    ImageJob job;
    QString destinationPath;
    bool overwriteDestination{false};
};

struct ProcessingResult {
    bool succeeded{false};
    QString sourcePath;
    QString outputPath;
    QString failureStage;
    QString errorMessage;
};

class ImageProcessor final {
public:
    static bool isBackendAvailable();
    static QString backendExecutable();
    static QString metadataExecutable();
    static ImageMetadata readMetadata(const QString &path);
    static qint64 estimateOutputSize(const ImageJob &job, const QString &suffix);
    ProcessingResult process(const ProcessingRequest &request) const;
};

} // namespace papercutter
