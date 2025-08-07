#include "CompressionUtils.h"

#include "common/Exception.h"

#include <zlib.h>

std::vector<char> CompressionUtils::compressData(const void* source, const size_t sourceLength, const int compressionLevel) {
    const auto sourceData = static_cast<const Bytef *>(source);
    uLongf destinationLength = compressBound(sourceLength);
    std::vector<char> destinationBuffer(destinationLength);
    auto *destinationData = reinterpret_cast<Bytef *>(destinationBuffer.data());

    const int result = compress2(destinationData, &destinationLength, sourceData, sourceLength, compressionLevel);
    if (result != Z_OK) {
        throw Exception("Failed to compress data: {}", zError(result));
    }

    return destinationBuffer;
}

std::vector<char> CompressionUtils::decompressData(const std::vector<char>& source, size_t destinationLength) {
    const auto sourceData = reinterpret_cast<const Bytef *>(source.data());
    unsigned long sourceLength = source.size();
    std::vector<char> destinationBuffer(destinationLength);
    auto *destinationData = reinterpret_cast<Bytef *>(destinationBuffer.data());

    const int result = uncompress2(destinationData, &destinationLength, sourceData, &sourceLength);
    if (result != Z_OK) {
        throw Exception("Failed to decompress data: {}", zError(result));
    }

    return destinationBuffer;
}

