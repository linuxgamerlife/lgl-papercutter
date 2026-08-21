#pragma once

#include "app/backupservice.h"
#include "core/imagejob.h"

#include <QString>

namespace papercutter {

enum class ProcessingOperation {
    AcceptAndReplace,
    SaveAs,
};

struct ProcessingRequest {
    ImageJob job;
    ProcessingOperation operation{ProcessingOperation::AcceptAndReplace};
    QString destinationPath;
    QString backupFolder;
    bool overwriteDestination{false};
};

struct ProcessingResult {
    bool succeeded{false};
    ProcessingOperation operation{ProcessingOperation::AcceptAndReplace};
    QString sourcePath;
    QString outputPath;
    QString backupPath;
    BackupRecord backupRecord;
    QString failureStage;
    QString errorMessage;
};

class ImageProcessor final {
public:
    static bool isBackendAvailable();
    static QString backendExecutable();
    ProcessingResult process(const ProcessingRequest &request) const;
};

} // namespace papercutter
