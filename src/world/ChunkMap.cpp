#include "ChunkMap.h"

#include "../Log.h"

Chunk & ChunkMap::getChunk(const glm::ivec3 key) {
    if (const auto chunk = tryGetChunk(key)) {
        return *chunk;
    }

    return createChunk(key);
}

Chunk * ChunkMap::tryGetChunk(const glm::ivec3 key) {
    const auto it = m_Chunks.find(key);
    return it != m_Chunks.end() ? &it->second : nullptr;
}

const Chunk * ChunkMap::tryGetChunk(const glm::ivec3 key) const {
    const auto it = m_Chunks.find(key);
    return it != m_Chunks.end() ? &it->second : nullptr;
}

bool ChunkMap::hasChunk(const glm::ivec3 key) const {
    return m_Chunks.contains(key);
}

void ChunkMap::moveChunk(Chunk &chunk) {
    const auto key = chunk.getPosition();
    m_Chunks.try_emplace(key, std::move(chunk));
    setNeighboursDirty(key);
}

void ChunkMap::removeChunk(const glm::ivec3 key) {
    m_Chunks.erase(key);
}

VoxelData ChunkMap::getVoxel(const WorldCoordinate &coord) const {
    const auto cPos = coord.chunkPosition();
    const auto lPos = coord.localPosition();

    if (const auto chunk = tryGetChunk(cPos)) {
        return chunk->getVoxel(lPos.x, lPos.y, lPos.z);
    }

    return VoxelData{};
}

bool ChunkMap::hasVoxel(const WorldCoordinate &coord) const {
    const auto cPos = coord.chunkPosition();
    const auto lPos = coord.localPosition();

    if (const auto chunk = tryGetChunk(cPos)) {
        return chunk->hasVoxel(lPos.x, lPos.y, lPos.z);
    }

    return false;
}

void ChunkMap::setVoxel(const WorldCoordinate &coord, const VoxelData &voxel) {
    const auto cPos = coord.chunkPosition();
    const auto lPos = coord.localPosition();

    getChunk(cPos).setVoxel(voxel, lPos.x, lPos.y, lPos.z);

    setNeighboursDirtyIfEdge(cPos, lPos);
}

void ChunkMap::removeVoxel(const WorldCoordinate &coord) {
    const auto cPos = coord.chunkPosition();
    const auto lPos = coord.localPosition();

    if (const auto chunk = tryGetChunk(cPos)) {
        chunk->setVoxel(VoxelData{}, lPos.x, lPos.y, lPos.z);

        setNeighboursDirtyIfEdge(cPos, lPos);
    }
}

Chunk & ChunkMap::createChunk(const glm::ivec3 &key) {
    auto [it, success] = m_Chunks.try_emplace(key, key);

    if (success) {
        LogApp::info("Created chunk at ({}, {}, {})", key.x, key.y, key.z);

        setNeighboursDirty(key);
    } else {
        LogApp::error("Failed to create chunk at ({}, {}, {})", key.x, key.y, key.z);
    }

    return it->second;
}

void ChunkMap::setChunkDirty(const glm::ivec3 &key) {
    if (const auto chunk = tryGetChunk(key)) {
        chunk->setDirty();
    }
}

void ChunkMap::setNeighboursDirty(const glm::ivec3 &key) {
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                if (x == 0 && y == 0 && z == 0) {
                    continue;
                }
                setChunkDirty(key + glm::ivec3(x, y, z));
            }
        }
    }
}

void ChunkMap::setNeighboursDirtyIfEdge(const glm::ivec3 &key, const glm::ivec3 &pos) {
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

    if (pos.x == 0 && pos.y == 0) {
        setChunkDirty(key + glm::ivec3(-1, -1, 0));
    }
    if (pos.x == 0 && pos.y == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(-1, 1, 0));
    }
    if (pos.x == 0 && pos.z == 0) {
        setChunkDirty(key + glm::ivec3(-1, 0, -1));
    }
    if (pos.x == 0 && pos.z == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(-1, 0, 1));
    }
    if (pos.x == Chunk::SIZE - 1 && pos.y == 0) {
        setChunkDirty(key + glm::ivec3(1, -1, 0));
    }
    if (pos.x == Chunk::SIZE - 1 && pos.y == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(1, 1, 0));
    }
    if (pos.x == Chunk::SIZE - 1 && pos.z == 0) {
        setChunkDirty(key + glm::ivec3(1, 0, -1));
    }
    if (pos.x == Chunk::SIZE - 1 && pos.z == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(1, 0, 1));
    }
    if (pos.y == 0 && pos.z == 0) {
        setChunkDirty(key + glm::ivec3(0, -1, -1));
    }
    if (pos.y == 0 && pos.z == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(0, -1, 1));
    }
    if (pos.y == Chunk::SIZE - 1 && pos.z == 0) {
        setChunkDirty(key + glm::ivec3(0, 1, -1));
    }
    if (pos.y == Chunk::SIZE - 1 && pos.z == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(0, 1, 1));
    }


    if (pos.x == 0 && pos.y == 0 && pos.z == 0) {
        setChunkDirty(key + glm::ivec3(-1, -1, -1));
    }
    if (pos.x == 0 && pos.y == 0 && pos.z == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(-1, -1, 1));
    }
    if (pos.x == 0 && pos.y == Chunk::SIZE - 1 && pos.z == 0) {
        setChunkDirty(key + glm::ivec3(-1, 1, -1));
    }
    if (pos.x == 0 && pos.y == Chunk::SIZE - 1 && pos.z == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(-1, 1, 1));
    }
    if (pos.x == Chunk::SIZE - 1 && pos.y == 0 && pos.z == 0) {
        setChunkDirty(key + glm::ivec3(1, -1, -1));
    }
    if (pos.x == Chunk::SIZE - 1 && pos.y == 0 && pos.z == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(1, -1, 1));
    }
    if (pos.x == Chunk::SIZE - 1 && pos.y == Chunk::SIZE - 1 && pos.z == 0) {
        setChunkDirty(key + glm::ivec3(1, 1, -1));
    }
    if (pos.x == Chunk::SIZE - 1 && pos.y == Chunk::SIZE - 1 && pos.z == Chunk::SIZE - 1) {
        setChunkDirty(key + glm::ivec3(1, 1, 1));
    }
}
