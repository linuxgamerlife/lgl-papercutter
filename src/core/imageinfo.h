#pragma once

#include "core/composition.h"

#include <optional>
#include <string>

namespace papercutter {

// Pixel dimensions read from a PNG, JPEG or WebP header, identified by content rather than
// extension. JPEG EXIF orientations 5–8 swap width and height so the result matches the
// image as displayed (and as ImageMagick's -auto-orient renders it).
std::optional<Size> readImageDimensions(const std::string &path);

// True when a PNG file uses colour type 3 (palette).
bool isIndexedPng(const std::string &path);

} // namespace papercutter
