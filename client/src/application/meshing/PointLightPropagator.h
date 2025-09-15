#pragma once

#include <array>
#include <vector>
#include <cstdint>

#include "application/world/chunk/Chunk.h"
#include "application/world/chunk/ChunkNeighborhood.h"

class PointLightPropagator {
public:
    static constexpr int DIM = Chunk::WIDTH * 3;

    struct LightMap {
        std::array<uint8_t, DIM * DIM * DIM> data{};
        uint8_t& operator[](const size_t index) { return data[index]; }
        const uint8_t& operator[](const size_t index) const { return data[index]; }
        uint8_t* begin() { return data.data(); }
        uint8_t* end() { return data.data() + data.size(); }
        const uint8_t* begin() const { return data.data(); }
        const uint8_t* end() const { return data.data() + data.size(); }

        BlockLightInfo getLightInfo(const int x, const int y, const int z) const {
            return BlockLightInfo(data[x * DIM * DIM + y * DIM + z]);
        }
    };

    static const LightMap& compute(const ChunkNeighborhood& neighborChunks,
                                   const std::vector<Chunk::LightSource>& lights);
};

