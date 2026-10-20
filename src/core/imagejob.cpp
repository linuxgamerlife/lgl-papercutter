#include "core/imagejob.h"

#include <glibmm/miscutils.h>

#include <algorithm>
#include <cctype>

namespace papercutter {

std::string ImageJob::displayName() const
{
    return Glib::path_get_basename(sourcePath);
}

std::string rawFileSuffix(const std::string &path)
{
    const std::string name = Glib::path_get_basename(path);
    const auto dot = name.rfind('.');
    return dot == std::string::npos ? std::string() : name.substr(dot + 1);
}

std::string fileSuffix(const std::string &path)
{
    std::string suffix = rawFileSuffix(path);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return suffix;
}

bool isSupportedSuffix(const std::string &suffix)
{
    return suffix == "jpg" || suffix == "jpeg" || suffix == "png" || suffix == "webp";
}

} // namespace papercutter
