#pragma once

#include <cstddef>
#include <set>
#include <string>

namespace papercutter {

// Numbered filenames ("0312.jpg") already present in an export folder. Exports fill the
// gaps from the lowest number upwards before continuing after the highest.
struct DestinationNumbering {
    bool foundNumberedFile{false};
    unsigned long long lowestNumber{0};
    unsigned long long highestNumber{0};
    unsigned long long nextCandidate{0};
    std::size_t paddingWidth{0};
    std::set<unsigned long long> usedNumbers;
};

// Regular files in folder (not recursive) whose complete base name is all digits.
DestinationNumbering inspectDestinationNumbering(const std::string &folder);

// The next unused number at or after nextCandidate; false when none was found or the
// range is exhausted.
bool takeNextAvailableNumber(DestinationNumbering &numbering, unsigned long long &number);

// number zero-padded to width digits.
std::string paddedNumber(unsigned long long number, std::size_t width);

// "<padded number>.<suffix>", or just the number when suffix is empty.
std::string numberedFileName(unsigned long long number, std::size_t width,
                             const std::string &suffix);

} // namespace papercutter
