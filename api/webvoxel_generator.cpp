#include "webvoxel_generator.h"

#include "application/domain/BlockId.h"
#include "application/world/WorldGenerator.h"
#include "application/world/chunk/Chunk.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

struct WebVoxelGenerator {
    explicit WebVoxelGenerator(const int32_t seed, const bool cavesEnabled)
        : seed(seed), generator({.seed = seed, .cavesEnabled = cavesEnabled}) {}

    int32_t seed;
    WorldGenerator generator;
    std::mutex mutex;
};

namespace {

thread_local std::string lastError;

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

std::vector<uint8_t> serializeJson(const Chunk& chunk, const int32_t seed, const int32_t x, const int32_t y, const int32_t z) {
    std::ostringstream stream;
    stream << "{\"seed\":" << seed
           << ",\"chunk\":{\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z << "}"
           << ",\"width\":" << Chunk::WIDTH
           << ",\"voxels\":[";

    bool first = true;
    for (uint32_t voxelX = 0; voxelX < Chunk::WIDTH; ++voxelX) {
        for (uint32_t voxelY = 0; voxelY < Chunk::WIDTH; ++voxelY) {
            for (uint32_t voxelZ = 0; voxelZ < Chunk::WIDTH; ++voxelZ) {
                const auto& voxel = chunk.getVoxel(voxelX, voxelY, voxelZ);
                if (voxel.isEmpty()) continue;
                if (!first) stream << ',';
                first = false;
                const auto block = voxel.getBlockId();
                stream << "{\"x\":" << voxelX
                       << ",\"y\":" << voxelY
                       << ",\"z\":" << voxelZ
                       << ",\"block\":\"" << blockName(block)
                       << "\",\"blockId\":" << static_cast<uint32_t>(block) << '}';
            }
        }
    }
    stream << "]}\n";
    const auto value = stream.str();
    return {value.begin(), value.end()};
}

} // namespace

WebVoxelGenerator* webvoxel_generator_create(const int32_t seed, const int32_t caves_enabled) {
    try {
        lastError.clear();
        return new WebVoxelGenerator(seed, caves_enabled != 0);
    } catch (const std::exception& error) {
        lastError = error.what();
    } catch (...) {
        lastError = "Unknown error while creating the world generator.";
    }
    return nullptr;
}

void webvoxel_generator_destroy(WebVoxelGenerator* generator) {
    delete generator;
}

int32_t webvoxel_generate_chunk(WebVoxelGenerator* generator, const int32_t x, const int32_t y, const int32_t z,
                                const int32_t format, uint8_t** output, size_t* output_size) {
    if (generator == nullptr || output == nullptr || output_size == nullptr) {
        lastError = "Generator, output, and output_size must not be null.";
        return 1;
    }
    *output = nullptr;
    *output_size = 0;
    if (format != 0 && format != 1) {
        lastError = "Output format must be 0 (JSON) or 1 (binary).";
        return 2;
    }

    try {
        lastError.clear();
        std::scoped_lock lock(generator->mutex);
        Chunk chunk(x, y, z);
        chunk.generate(generator->generator);

        std::vector<uint8_t> data;
        if (format == 1) {
            const auto compressed = chunk.serializeCompressed();
            data.assign(compressed.begin(), compressed.end());
        } else {
            data = serializeJson(chunk, generator->seed, x, y, z);
        }

        auto* memory = static_cast<uint8_t*>(std::malloc(data.size()));
        if (memory == nullptr && !data.empty()) {
            lastError = "Unable to allocate the chunk response buffer.";
            return 3;
        }
        if (!data.empty()) {
            std::memcpy(memory, data.data(), data.size());
        }
        *output = memory;
        *output_size = data.size();
        return 0;
    } catch (const std::exception& error) {
        lastError = error.what();
    } catch (...) {
        lastError = "Unknown error while generating a chunk.";
    }
    return 4;
}

void webvoxel_free(void* memory) {
    std::free(memory);
}

const char* webvoxel_last_error() {
    return lastError.c_str();
}
