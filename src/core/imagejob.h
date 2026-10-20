#pragma once

#include "core/composition.h"

#include <string>

namespace papercutter {

struct ImageMetadata {
    std::string description;
    std::string artist;
    std::string copyright;
    std::string comment;

    bool isEmpty() const
    {
        return description.empty() && artist.empty()
            && copyright.empty() && comment.empty();
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
    std::string sourcePath;
    Size sourceSize;
    CompositionState composition;
    int outputQuality{95};
    ImageMetadata metadata;
    bool metadataEdited{false};
    JobStatus status{JobStatus::AwaitingReview};
    std::string errorMessage;

    std::string displayName() const;
};

// Extension without the dot ("" when there is none), like QFileInfo::suffix().
std::string rawFileSuffix(const std::string &path);

// rawFileSuffix() in lower case.
std::string fileSuffix(const std::string &path);

// True for the formats Papercutter reads and writes: jpg, jpeg, png and webp.
bool isSupportedSuffix(const std::string &suffix);

} // namespace papercutter
