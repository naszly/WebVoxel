#pragma once

#include <ranges>
#include <glm/gtx/hash.hpp>
#include <unordered_map>

#include "WorldCoordinate.h"
#include "../Log.h"

class ChunkMap {
public:
    ChunkMap() = default;
    ~ChunkMap() = default;

    ChunkMap(const ChunkMap&) = delete;
    ChunkMap(ChunkMap&&) = delete;
    ChunkMap& operator=(const ChunkMap&) = delete;
    ChunkMap& operator=(ChunkMap&&) = delete;

    [[nodiscard]] Chunk& getChunk(const glm::ivec3 key) {
        if (const auto chunk = tryGetChunk(key)) {
            return *chunk;
        }

        return createChunk(key);
    }

    [[nodiscard]] Chunk* tryGetChunk(const glm::ivec3 key) {
        if (const auto it = m_Chunks.find(key); it != m_Chunks.end()) {
            return &it->second;
        }

        return nullptr;
    }

    [[nodiscard]] bool hasChunk(const glm::ivec3 key) const {
        return m_Chunks.contains(key);
    }

    auto getChunks() {
        return m_Chunks | std::ranges::views::values;
    }

    VoxelData getVoxel(const WorldCoordinate &coord) {
        const auto cPos = coord.chunkPosition();
        const auto lPos = coord.localPosition();

        if (const auto chunk = tryGetChunk(cPos)) {
            return chunk->getVoxel(lPos.x, lPos.y, lPos.z);
        }

        return VoxelData{};
    }

    void setVoxel(const WorldCoordinate &coord, const VoxelData &voxel) {
        const auto cPos = coord.chunkPosition();
        const auto lPos = coord.localPosition();

        getChunk(cPos).setVoxel(voxel, lPos.x, lPos.y, lPos.z);

        setNeighboursDirtyIfEdge(cPos, lPos);
    }

    void removeVoxel(const WorldCoordinate &coord) {
        const auto cPos = coord.chunkPosition();
        const auto lPos = coord.localPosition();

        if (const auto chunk = tryGetChunk(cPos)) {
            chunk->setVoxel(VoxelData{}, lPos.x, lPos.y, lPos.z);

            setNeighboursDirtyIfEdge(cPos, lPos);
        }
    }

private:
    std::unordered_map<glm::ivec3, Chunk> m_Chunks;

    Chunk& createChunk(const glm::ivec3 &key) {
        auto [it, success] = m_Chunks.try_emplace(key, key);

        if (success) {
            LogApp::info("Created chunk at ({}, {}, {})", key.x, key.y, key.z);

            setNeighboursDirty(key);
        } else {
            LogApp::error("Failed to create chunk at ({}, {}, {})", key.x, key.y, key.z);
        }

        return it->second;
    }

    void setChunkDirty(const glm::ivec3& key) {
        if (const auto neighbor = tryGetChunk(key)) {
            neighbor->setDirty();
        }
    }

    void setNeighboursDirty(const glm::ivec3& key) {
        setChunkDirty(key + glm::ivec3(-1, 0, 0));
        setChunkDirty(key + glm::ivec3(1, 0, 0));
        setChunkDirty(key + glm::ivec3(0, -1, 0));
        setChunkDirty(key + glm::ivec3(0, 1, 0));
        setChunkDirty(key + glm::ivec3(0, 0, -1));
        setChunkDirty(key + glm::ivec3(0, 0, 1));
    }

    void setNeighboursDirtyIfEdge(const glm::ivec3& key, const glm::ivec3& pos) {
        if (pos.x == 0) {
            setChunkDirty(key + glm::ivec3(-1, 0, 0));
        }
        if (pos.x == Chunk::SIZE - 1) {
            setChunkDirty(key + glm::ivec3(1, 0, 0));
        }
        if (pos.y == 0) {
            setChunkDirty(key + glm::ivec3(0, -1, 0));
        }
        if (pos.y == Chunk::SIZE - 1) {
            setChunkDirty(key + glm::ivec3(0, 1, 0));
        }
        if (pos.z == 0) {
            setChunkDirty(key + glm::ivec3(0, 0, -1));
        }
        if (pos.z == Chunk::SIZE - 1) {
            setChunkDirty(key + glm::ivec3(0, 0, 1));
        }
    }
};
