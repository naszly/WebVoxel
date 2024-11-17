#pragma once
#include <cassert>
#include <optional>

#include "WorldCoordinate.h"
#include "../Log.h"

template <int SIZE_X, int SIZE_Y, int SIZE_Z>
class ChunkMap {
public:
    ChunkMap() = default;
    ~ChunkMap() = default;

    ChunkMap(const ChunkMap&) = delete;
    ChunkMap(ChunkMap&&) = delete;
    ChunkMap& operator=(const ChunkMap&) = delete;
    ChunkMap& operator=(ChunkMap&&) = delete;

    Chunk& get(const int x, const int y, const int z) {
        assert(x >= 0 && x < SIZE_X);
        assert(y >= 0 && y < SIZE_Y);
        assert(z >= 0 && z < SIZE_Z);

        if (!m_Chunks[x + SIZE_X * (y + SIZE_Y * z)].has_value()) {
            create(x, y, z);
        }

        return m_Chunks[x + SIZE_X * (y + SIZE_Y * z)].value();
    }

    [[nodiscard]] auto begin() {
        return m_Chunks;
    }

    [[nodiscard]] auto end() {
        return m_Chunks + MAP_SIZE;
    }

    VoxelData getVoxel(const WorldCoordinate &coord) {
        const auto pos = coord.worldPosition();
        const auto cPos = coord.chunkPosition();
        const auto lPos = coord.localPosition();

        LogApp::info("ChunkMap::getVoxel: Pos: ({}, {}, {}) cPos: ({}, {}, {}), lPos: ({}, {}, {})",
            pos.x, pos.y, pos.z, cPos.x, cPos.y, cPos.z, lPos.x, lPos.y, lPos.z);

        if (cPos.x < 0 || cPos.x >= SIZE_X || cPos.y < 0 || cPos.y >= SIZE_Y || cPos.z < 0 || cPos.z >= SIZE_Z) {
            return VoxelData{};
        }

        if (const auto &chunk = m_Chunks[cPos.x + SIZE_X * (cPos.y + SIZE_Y * cPos.z)]) {
            return chunk->get(lPos.x, lPos.y, lPos.z);
        }

        return VoxelData{};
    }

    void setVoxel(const WorldCoordinate &coord, const VoxelData &voxel) {
        const auto pos = coord.worldPosition();
        const auto cPos = coord.chunkPosition();
        const auto lPos = coord.localPosition();

        LogApp::info("ChunkMap::setVoxel: Pos: ({}, {}, {}) cPos: ({}, {}, {}), lPos: ({}, {}, {})",
            pos.x, pos.y, pos.z, cPos.x, cPos.y, cPos.z, lPos.x, lPos.y, lPos.z);

        if (cPos.x < 0 || cPos.x >= SIZE_X || cPos.y < 0 || cPos.y >= SIZE_Y || cPos.z < 0 || cPos.z >= SIZE_Z) {
            LogApp::info("ChunkMap::setVoxel: Chunk out of bounds");
            return;
        }

        auto &chunk = get(cPos.x, cPos.y, cPos.z);

        chunk.set(voxel, lPos.x, lPos.y, lPos.z);
    }

private:
    static constexpr size_t MAP_SIZE = SIZE_X * SIZE_Y * SIZE_Z;
    std::optional<Chunk> m_Chunks[MAP_SIZE];

    void create(const int x, const int y, const int z) {
        assert(x >= 0 && x < SIZE_X);
        assert(y >= 0 && y < SIZE_Y);
        assert(z >= 0 && z < SIZE_Z);

        assert(!m_Chunks[x + SIZE_X * (y + SIZE_Y * z)].has_value());

        m_Chunks[x + SIZE_X * (y + SIZE_Y * z)].emplace(x, y, z);
    }

    void remove(const int x, const int y, const int z) {
        assert(x >= 0 && x < SIZE_X);
        assert(y >= 0 && y < SIZE_Y);
        assert(z >= 0 && z < SIZE_Z);

        m_Chunks[x + SIZE_X * (y + SIZE_Y * z)] = std::nullopt;
    }
};
