#pragma once

#include <vector>

class CompressionUtils {
public:
    [[nodiscard]] static std::vector<char> compressData(const void* source, size_t sourceLength, int compressionLevel = 9);
    static std::vector<char> decompressData(const std::vector<char>& source, size_t destinationLength);
};

