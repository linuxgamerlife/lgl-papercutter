#pragma once

#include <QPointF>
#include <QSize>

namespace papercutter {

struct CompositionState {
    QSize targetSize{3440, 1440};
    QPointF normalizedOffset{0.0, 0.0};
    double zoom{1.0};

    bool isValid() const;
    double baseScaleFor(const QSize &sourceSize) const;
    double effectiveScaleFor(const QSize &sourceSize) const;
    bool isUpscaled(const QSize &sourceSize) const;
};

} // namespace papercutter
