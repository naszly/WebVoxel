#include "ChunkMap.h"

#include "common/Log.h"

Chunk & ChunkMap::getChunk(const glm::ivec3 key) {
    if (const auto chunk = tryGetChunk(key)) {
        return *chunk;
    }

    return createChunk(key);
}

Chunk * ChunkMap::tryGetChunk(const glm::ivec3 key) {
    const auto it = m_chunks.find(key);
    return it != m_chunks.end() ? &it->second : nullptr;
}

const Chunk * ChunkMap::tryGetChunk(const glm::ivec3 key) const {
    const auto it = m_chunks.find(key);
    return it != m_chunks.end() ? &it->second : nullptr;
}

bool ChunkMap::hasChunk(const glm::ivec3 key) const {
    return m_chunks.contains(key);
}

Chunk & ChunkMap::createChunk(const glm::ivec3 &key) {
    auto [it, success] = m_chunks.try_emplace(key, key);

    if (success) {
        LogApp::info("Created chunk at ({}, {}, {})", key.x, key.y, key.z);

        setNeighboursDirty(key);
    } else {
        LogApp::error("Failed to create chunk at ({}, {}, {})", key.x, key.y, key.z);
    }

    return it->second;
}

void ChunkMap::insertChunkByMove(Chunk &chunk) {
    const auto key = chunk.getPosition();
    m_chunks.try_emplace(key, std::move(chunk));
    setNeighboursDirty(key);
}

Chunk ChunkMap::extractChunkByMove(const glm::ivec3& key) {
    const auto it = m_chunks.find(key);
    if (it == m_chunks.end()) {
        LogApp::error("Chunk at ({}, {}, {}) not found for moving out", key.x, key.y, key.z);
        return Chunk(key);
    }
    Chunk movedChunk = std::move(it->second);
    m_chunks.erase(it);
    return movedChunk;
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

    setNeighboursDirty(cPos);
}

Chunk* ChunkMap::queueVoxelToSet(const WorldCoordinate& coord, const VoxelData& voxel) {
    const auto cPos = coord.chunkPosition();
    const auto lPos = coord.localPosition();

    Chunk& chunk = getChunk(cPos);
    chunk.queueVoxelToSet(voxel, lPos.x, lPos.y, lPos.z);

    return &chunk;
}

void ChunkMap::executeQueuedVoxelsToSet(Chunk* chunk) {
    setNeighboursDirty(chunk->getPosition());
    chunk->executeQueuedVoxelsToSet();
}

void ChunkMap::setChunkDirty(const glm::ivec3 &key) {
    if (const auto neighbor = tryGetChunk(key)) {
        neighbor->setGpuBufferDirty();
    }
}

void ChunkMap::setNeighboursDirty(const glm::ivec3 &key) {
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dy == 0 && dz == 0) continue;

                setChunkDirty(key + glm::ivec3(dx, dy, dz));
            }
        }
    }
}