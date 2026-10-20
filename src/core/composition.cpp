#include "core/composition.h"

#include <algorithm>

namespace papercutter {

bool CompositionState::isValid() const
{
    return targetSize.width > 0
        && targetSize.height > 0
        && zoom > 0.0
        && normalizedOffset.x >= -1.0
        && normalizedOffset.x <= 1.0
        && normalizedOffset.y >= -1.0
        && normalizedOffset.y <= 1.0;
}

double CompositionState::baseScaleFor(const Size &sourceSize) const
{
    if (!isValid() || sourceSize.width <= 0 || sourceSize.height <= 0)
        return 0.0;

    const double scaleX = static_cast<double>(targetSize.width) / sourceSize.width;
    const double scaleY = static_cast<double>(targetSize.height) / sourceSize.height;
    return std::max(scaleX, scaleY);
}

double CompositionState::effectiveScaleFor(const Size &sourceSize) const
{
    return baseScaleFor(sourceSize) * zoom;
}

bool CompositionState::isUpscaled(const Size &sourceSize) const
{
    return effectiveScaleFor(sourceSize) > 1.0;
}

} // namespace papercutter
