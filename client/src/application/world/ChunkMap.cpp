#include "ChunkMap.h"

#include "common/Log.h"

Chunk& ChunkMap::getChunk(const glm::ivec3 key) {
    if (auto* chunk = tryGetChunk(key)) {
        return *chunk;
    }

    return createChunk(key);
}

Chunk* ChunkMap::tryGetChunk(const glm::ivec3 key) {
    auto& slot = m_chunks[packIndex(key)];
    return slot ? &*slot : nullptr;
}

const Chunk* ChunkMap::tryGetChunk(const glm::ivec3 key) const {
    const auto& slot = m_chunks[packIndex(key)];
    return slot ? &*slot : nullptr;
}

bool ChunkMap::hasChunk(const glm::ivec3 key) const {
    return m_chunks[packIndex(key)].has_value();
}

Chunk& ChunkMap::createChunk(const glm::ivec3 &key) {
    auto& slot = m_chunks[packIndex(key)];
    if (!slot) {
        slot.emplace(key);
        LogApp::info("Created chunk at ({}, {}, {})", key.x, key.y, key.z);
        setNeighboursDirty(key);
    } else {
        LogApp::error("Failed to create chunk at ({}, {}, {})", key.x, key.y, key.z);
    }
    return *slot;
}

void ChunkMap::insertChunkByMove(Chunk &chunk) {
    const auto key = chunk.getPosition();
    m_chunks[packIndex(key)] = std::move(chunk);
    setNeighboursDirty(key);
}

Chunk ChunkMap::extractChunkByMove(const glm::ivec3& key) {
    auto& slot = m_chunks[packIndex(key)];
    if (!slot) {
        LogApp::error("Chunk at ({}, {}, {}) not found for moving out", key.x, key.y, key.z);
        return Chunk(key);
    }
    Chunk moved = std::move(*slot);
    slot.reset();
    return moved;
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

void ChunkMap::clear() {
    for (auto& slot : m_chunks) {
        slot.reset();
    }
}

void ChunkMap::setChunkDirty(const glm::ivec3 &key) {
    if (auto* neighbor = tryGetChunk(key)) {
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