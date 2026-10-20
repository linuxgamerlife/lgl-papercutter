#include "core/imageinfo.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace papercutter {

namespace {

constexpr unsigned char pngSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

std::uint32_t bigEndian32(const unsigned char *data)
{
    return (std::uint32_t{data[0]} << 24) | (std::uint32_t{data[1]} << 16)
        | (std::uint32_t{data[2]} << 8) | std::uint32_t{data[3]};
}

std::uint16_t read16(const unsigned char *data, const bool littleEndian)
{
    return littleEndian ? static_cast<std::uint16_t>(data[0] | (data[1] << 8))
                        : static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

std::uint32_t read32(const unsigned char *data, const bool littleEndian)
{
    if (!littleEndian)
        return bigEndian32(data);
    return std::uint32_t{data[0]} | (std::uint32_t{data[1]} << 8)
        | (std::uint32_t{data[2]} << 16) | (std::uint32_t{data[3]} << 24);
}

// Orientation (tag 0x0112) from IFD0 of an EXIF APP1 payload; 1 when absent.
int exifOrientation(const std::vector<unsigned char> &segment)
{
    static const unsigned char exifHeader[6] = {'E', 'x', 'i', 'f', 0, 0};
    if (segment.size() < 14 || std::memcmp(segment.data(), exifHeader, 6) != 0)
        return 1;
    const unsigned char *tiff = segment.data() + 6;
    const std::size_t size = segment.size() - 6;
    bool littleEndian = false;
    if (tiff[0] == 'I' && tiff[1] == 'I')
        littleEndian = true;
    else if (!(tiff[0] == 'M' && tiff[1] == 'M'))
        return 1;
    if (read16(tiff + 2, littleEndian) != 42)
        return 1;
    const std::uint32_t ifd = read32(tiff + 4, littleEndian);
    if (ifd + 2 > size)
        return 1;
    const std::uint16_t entries = read16(tiff + ifd, littleEndian);
    for (std::uint16_t i = 0; i < entries; ++i) {
        const std::size_t entry = ifd + 2 + std::size_t{i} * 12;
        if (entry + 12 > size)
            return 1;
        if (read16(tiff + entry, littleEndian) == 0x0112) {
            const int value = read16(tiff + entry + 8, littleEndian);
            return value >= 1 && value <= 8 ? value : 1;
        }
    }
    return 1;
}

std::optional<Size> jpegDimensions(std::ifstream &file)
{
    int orientation = 1;
    file.seekg(2);
    while (file) {
        int byte = file.get();
        if (byte != 0xff)
            return std::nullopt;
        int marker = file.get();
        while (marker == 0xff)
            marker = file.get();
        if (marker == EOF)
            return std::nullopt;
        if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd8))
            continue;
        unsigned char lengthBytes[2];
        if (!file.read(reinterpret_cast<char *>(lengthBytes), 2))
            return std::nullopt;
        const int length = (lengthBytes[0] << 8) | lengthBytes[1];
        if (length < 2)
            return std::nullopt;
        const bool startOfFrame = marker >= 0xc0 && marker <= 0xcf
            && marker != 0xc4 && marker != 0xc8 && marker != 0xcc;
        if (startOfFrame) {
            unsigned char frame[5];
            if (!file.read(reinterpret_cast<char *>(frame), 5))
                return std::nullopt;
            Size size{(frame[3] << 8) | frame[4], (frame[1] << 8) | frame[2]};
            if (!size.isValid())
                return std::nullopt;
            if (orientation >= 5)
                std::swap(size.width, size.height);
            return size;
        }
        if (marker == 0xda || marker == 0xd9)
            return std::nullopt;
        if (marker == 0xe1 && orientation == 1) {
            std::vector<unsigned char> segment(static_cast<std::size_t>(length - 2));
            if (!file.read(reinterpret_cast<char *>(segment.data()),
                           static_cast<std::streamsize>(segment.size())))
                return std::nullopt;
            orientation = exifOrientation(segment);
            continue;
        }
        file.seekg(length - 2, std::ios::cur);
    }
    return std::nullopt;
}

std::optional<Size> webpDimensions(const unsigned char *header, const std::size_t size)
{
    if (size < 30)
        return std::nullopt;
    const unsigned char *data = header + 20;
    Size result;
    if (std::memcmp(header + 12, "VP8 ", 4) == 0) {
        if (data[3] != 0x9d || data[4] != 0x01 || data[5] != 0x2a)
            return std::nullopt;
        result = {(data[6] | (data[7] << 8)) & 0x3fff, (data[8] | (data[9] << 8)) & 0x3fff};
    } else if (std::memcmp(header + 12, "VP8L", 4) == 0) {
        if (data[0] != 0x2f)
            return std::nullopt;
        result = {1 + (data[1] | ((data[2] & 0x3f) << 8)),
                  1 + ((data[2] >> 6) | (data[3] << 2) | ((data[4] & 0x0f) << 10))};
    } else if (std::memcmp(header + 12, "VP8X", 4) == 0) {
        result = {1 + (data[4] | (data[5] << 8) | (data[6] << 16)),
                  1 + (data[7] | (data[8] << 8) | (data[9] << 16))};
    } else {
        return std::nullopt;
    }
    if (!result.isValid())
        return std::nullopt;
    return result;
}

} // namespace

std::optional<Size> readImageDimensions(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;
    std::array<unsigned char, 32> header{};
    file.read(reinterpret_cast<char *>(header.data()), header.size());
    const auto read = static_cast<std::size_t>(file.gcount());
    file.clear();

    if (read >= 24 && std::memcmp(header.data(), pngSignature, 8) == 0
        && std::memcmp(header.data() + 12, "IHDR", 4) == 0) {
        const auto width = bigEndian32(header.data() + 16);
        const auto height = bigEndian32(header.data() + 20);
        if (width == 0 || height == 0 || width > 0x7fffffff || height > 0x7fffffff)
            return std::nullopt;
        return Size{static_cast<int>(width), static_cast<int>(height)};
    }
    if (read >= 3 && header[0] == 0xff && header[1] == 0xd8 && header[2] == 0xff)
        return jpegDimensions(file);
    if (read >= 16 && std::memcmp(header.data(), "RIFF", 4) == 0
        && std::memcmp(header.data() + 8, "WEBP", 4) == 0)
        return webpDimensions(header.data(), read);
    return std::nullopt;
}

bool isIndexedPng(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    unsigned char header[26];
    if (!file.read(reinterpret_cast<char *>(header), sizeof header))
        return false;
    return std::memcmp(header, pngSignature, 8) == 0 && header[25] == 3;
}

} // namespace papercutter
