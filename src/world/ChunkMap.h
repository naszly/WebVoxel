#pragma once

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

    Chunk& getChunk(const int x, const int y, const int z) {
        const auto key = glm::ivec3(x, y, z);
        const Chunk chunk(x, y, z);

        auto [it, inserted] = m_Chunks.try_emplace(key, x, y, z);

        if (inserted) {
            LogApp::info("Created chunk at ({}, {}, {})", x, y, z);
        }

        return it->second;
    }

    [[nodiscard]] Chunk* tryGetChunk(const int x, const int y, const int z) {
        const auto key = glm::ivec3(x, y, z);

        if (const auto it = m_Chunks.find(key); it != m_Chunks.end()) {
            return &it->second;
        }

        return nullptr;
    }

    std::vector<std::pair<glm::ivec3, Chunk &>> getChunks() {
        std::vector<std::pair<glm::ivec3, Chunk&>> chunks;
        for (auto it = m_Chunks.begin(); it != m_Chunks.end(); ) {
            auto &[key, chunk] = *it;
            if (chunk.isEmpty()) {
                it = m_Chunks.erase(it);
                LogApp::info("Removed chunk at ({}, {}, {})", key.x, key.y, key.z);
            } else {
                chunks.emplace_back(key, chunk);
                ++it;
            }
        }
        return chunks;
    }

    VoxelData getVoxel(const WorldCoordinate &coord) {
        const auto cPos = coord.chunkPosition();
        const auto lPos = coord.localPosition();

        if (const auto chunk = tryGetChunk(cPos.x, cPos.y, cPos.z)) {
            return chunk->getVoxel(lPos.x, lPos.y, lPos.z);
        }

        return VoxelData{};
    }

    void setVoxel(const WorldCoordinate &coord, const VoxelData &voxel) {
        if (voxel.isEmpty()) {
            removeVoxel(coord);
        } else {
            const auto cPos = coord.chunkPosition();
            const auto lPos = coord.localPosition();

            getChunk(cPos.x, cPos.y, cPos.z).setVoxel(voxel, lPos.x, lPos.y, lPos.z);
        }
    }

    void removeVoxel(const WorldCoordinate &coord) {
        const auto cPos = coord.chunkPosition();
        const auto lPos = coord.localPosition();

        if (const auto chunk = tryGetChunk(cPos.x, cPos.y, cPos.z)) {
            chunk->setVoxel(VoxelData{}, lPos.x, lPos.y, lPos.z);
        }
    }

private:
    std::unordered_map<glm::ivec3, Chunk> m_Chunks;
};
