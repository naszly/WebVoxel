#include "application/domain/BlockId.h"
#include "application/world/WorldGenerator.h"
#include "application/world/chunk/Chunk.h"

#include <charconv>
#include <iostream>
#include <string_view>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

namespace {

bool parseInt(const std::string_view value, int& result) {
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    return error == std::errc{} && end == value.data() + value.size();
}

const char* blockName(const BlockId block) {
    switch (block) {
        case BlockId::Grass: return "Grass";
        case BlockId::Dirt: return "Dirt";
        case BlockId::Stone: return "Stone";
        case BlockId::Duskstone: return "Duskstone";
        case BlockId::Blackrock: return "Blackrock";
        case BlockId::EclipseCrystal: return "EclipseCrystal";
        case BlockId::MoonlitLantern: return "MoonlitLantern";
        case BlockId::Air:
        case BlockId::Count:
            return "Air";
    }

    return "Unknown";
}

} // namespace

int main(const int argc, char** argv) {
    if (argc != 7) {
        std::cerr << "Usage: world-generator-api <seed> <chunk-x> <chunk-y> <chunk-z> <caves:true|false> <json|binary>\n";
        return 64;
    }

    int seed;
    int x;
    int y;
    int z;
    if (!parseInt(argv[1], seed) || !parseInt(argv[2], x) || !parseInt(argv[3], y) || !parseInt(argv[4], z)) {
        std::cerr << "Seed and chunk coordinates must be 32-bit integers.\n";
        return 64;
    }

    const std::string_view cavesArgument = argv[5];
    if (cavesArgument != "true" && cavesArgument != "false") {
        std::cerr << "The caves argument must be true or false.\n";
        return 64;
    }

    const std::string_view outputFormat = argv[6];
    if (outputFormat != "json" && outputFormat != "binary") {
        std::cerr << "The output format must be json or binary.\n";
        return 64;
    }

    WorldGenerator generator({.seed = seed, .cavesEnabled = cavesArgument == "true"});
    Chunk chunk(x, y, z);
    chunk.generate(generator);

    if (outputFormat == "binary") {
#if defined(_WIN32)
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        const auto data = chunk.serializeCompressed();
        std::cout.write(data.data(), static_cast<std::streamsize>(data.size()));
        return std::cout.good() ? 0 : 74;
    }

    std::cout << "{\"seed\":" << seed
              << ",\"chunk\":{\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z << "}"
              << ",\"width\":" << Chunk::WIDTH
              << ",\"voxels\":[";

    bool first = true;
    for (uint32_t voxelX = 0; voxelX < Chunk::WIDTH; ++voxelX) {
        for (uint32_t voxelY = 0; voxelY < Chunk::WIDTH; ++voxelY) {
            for (uint32_t voxelZ = 0; voxelZ < Chunk::WIDTH; ++voxelZ) {
                const auto& voxel = chunk.getVoxel(voxelX, voxelY, voxelZ);
                if (voxel.isEmpty()) {
                    continue;
                }

                if (!first) {
                    std::cout << ',';
                }
                first = false;

                const auto block = voxel.getBlockId();
                std::cout << "{\"x\":" << voxelX
                          << ",\"y\":" << voxelY
                          << ",\"z\":" << voxelZ
                          << ",\"block\":\"" << blockName(block)
                          << "\",\"blockId\":" << static_cast<uint32_t>(block)
                          << '}';
            }
        }
    }

    std::cout << "]}\n";
}
