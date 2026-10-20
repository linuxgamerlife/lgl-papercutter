#include "core/numbering.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <limits>

namespace papercutter {

namespace {

// QFileInfo::completeBaseName(): everything before the last dot.
std::string completeBaseName(const std::string &name)
{
    const auto dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

bool allDigits(const std::string &text)
{
    return !text.empty()
        && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

} // namespace

DestinationNumbering inspectDestinationNumbering(const std::string &folder)
{
    DestinationNumbering numbering;
    std::error_code error;
    std::filesystem::directory_iterator entries(folder, error);
    if (error)
        return numbering;
    for (const auto &entry : entries) {
        std::error_code statusError;
        if (!entry.is_regular_file(statusError))
            continue;
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.front() == '.')
            continue;
        const std::string stem = completeBaseName(name);
        if (!allDigits(stem))
            continue;
        errno = 0;
        char *end = nullptr;
        const unsigned long long number = std::strtoull(stem.c_str(), &end, 10);
        if (errno == ERANGE || end != stem.c_str() + stem.size())
            continue;
        if (!numbering.foundNumberedFile) {
            numbering.foundNumberedFile = true;
            numbering.lowestNumber = number;
            numbering.highestNumber = number;
        } else {
            numbering.lowestNumber = std::min(numbering.lowestNumber, number);
            numbering.highestNumber = std::max(numbering.highestNumber, number);
        }
        numbering.paddingWidth = std::max(numbering.paddingWidth, stem.size());
        numbering.usedNumbers.insert(number);
    }
    numbering.nextCandidate = numbering.lowestNumber;
    return numbering;
}

bool takeNextAvailableNumber(DestinationNumbering &numbering, unsigned long long &number)
{
    if (!numbering.foundNumberedFile)
        return false;
    constexpr auto maximum = std::numeric_limits<unsigned long long>::max();
    while (numbering.usedNumbers.count(numbering.nextCandidate) != 0) {
        if (numbering.nextCandidate == maximum)
            return false;
        ++numbering.nextCandidate;
    }
    number = numbering.nextCandidate;
    numbering.usedNumbers.insert(number);
    if (numbering.nextCandidate != maximum)
        ++numbering.nextCandidate;
    return true;
}

std::string paddedNumber(const unsigned long long number, const std::size_t width)
{
    std::string text = std::to_string(number);
    if (text.size() < width)
        text.insert(0, width - text.size(), '0');
    return text;
}

std::string numberedFileName(const unsigned long long number, const std::size_t width,
                             const std::string &suffix)
{
    const std::string stem = paddedNumber(number, width);
    return suffix.empty() ? stem : stem + '.' + suffix;
}

} // namespace papercutter
