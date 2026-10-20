#pragma once

#include "core/compositionstate.h"

#include <QSize>
#include <QString>

namespace papercutter {

struct ImageMetadata {
    QString description;
    QString artist;
    QString copyright;
    QString comment;

    bool isEmpty() const
    {
        return description.isEmpty() && artist.isEmpty()
            && copyright.isEmpty() && comment.isEmpty();
    }
};

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
    int outputQuality{95};
    ImageMetadata metadata;
    bool metadataEdited{false};
    JobStatus status{JobStatus::AwaitingReview};
    QString errorMessage;

    QString displayName() const;
};

} // namespace papercutter
