#pragma once

#include "core/compositionstate.h"

#include <QSize>
#include <QString>

namespace papercutter {

enum class JobStatus {
    AwaitingReview,
    Ready,
    Processing,
    Complete,
    Failed,
    Cancelled,
};

struct ImageJob {
    QString sourcePath;
    QSize sourceSize;
    CompositionState composition;
    JobStatus status{JobStatus::AwaitingReview};
    QString errorMessage;

    QString displayName() const;
};

} // namespace papercutter

