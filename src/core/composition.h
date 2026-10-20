#pragma once

namespace papercutter {

struct Size {
    int width{0};
    int height{0};

    bool isValid() const { return width > 0 && height > 0; }
    bool operator==(const Size &other) const
    {
        return width == other.width && height == other.height;
    }
    bool operator!=(const Size &other) const { return !(*this == other); }
};

struct PointF {
    double x{0.0};
    double y{0.0};
};

struct CompositionState {
    Size targetSize{3440, 1440};
    PointF normalizedOffset{0.0, 0.0};
    double zoom{1.0};

    bool isValid() const;
    double baseScaleFor(const Size &sourceSize) const;
    double effectiveScaleFor(const Size &sourceSize) const;
    bool isUpscaled(const Size &sourceSize) const;
};

} // namespace papercutter
