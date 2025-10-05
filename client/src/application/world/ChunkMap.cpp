#include "ChunkMap.h"

#include "common/Log.h"

Chunk& ChunkMap::getChunk(const glm::ivec3 key) {
    if (auto& chunk = tryGetChunk(key)) {
        return *chunk;
    }

    return createChunk(key);
}

std::optional<Chunk>& ChunkMap::tryGetChunk(const glm::ivec3 key) {
    return m_chunks[packIndex(key)];
}

const std::optional<Chunk>& ChunkMap::tryGetChunk(const glm::ivec3 key) const {
    return m_chunks[packIndex(key)];
}

Chunk* ChunkMap::tryGetChunkPtr(const glm::ivec3 key) {
    auto& slot = m_chunks[packIndex(key)];
    return slot ? &*slot : nullptr;
}

const Chunk* ChunkMap::tryGetChunkPtr(const glm::ivec3 key) const {
    const auto& slot = m_chunks[packIndex(key)];
    return slot ? &*slot : nullptr;
}

bool ChunkMap::hasChunk(const glm::ivec3 key) const {
    return m_chunkBitmap.test(packIndex(key));
}

bool ChunkMap::areChunkAndNeighborsPresent(const glm::ivec3& key) const {
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                const glm::ivec3 pos = key + glm::ivec3(dx, dy, dz);
                if (!hasChunk(pos)) {
                    return false;
                }
            }
        }
    }
    return true;
}

Chunk& ChunkMap::createChunk(const glm::ivec3 &key) {
    auto& slot = m_chunks[packIndex(key)];
    if (!slot) {
        slot.emplace(key);
        m_chunkBitmap.set(packIndex(key));
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
    m_chunkBitmap.set(packIndex(key));
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
    m_chunkBitmap.clear(packIndex(key));
    return moved;
}

std::vector<std::reference_wrapper<Chunk>> ChunkMap::getChunks() {
    std::vector<std::reference_wrapper<Chunk>> chunks;
    forEachChunk([&](const uint32_t i) {
        chunks.emplace_back(*m_chunks[i]);
    });
    return chunks;
}

std::vector<std::reference_wrapper<const Chunk>> ChunkMap::getChunks() const {
    std::vector<std::reference_wrapper<const Chunk>> chunks;
    forEachChunk([&](const uint32_t i) {
        chunks.emplace_back(*m_chunks[i]);
    });
    return chunks;
}

std::vector<std::reference_wrapper<Chunk>> ChunkMap::getChunksWithDirtyGpuBuffer() {
    std::vector<std::reference_wrapper<Chunk>> chunks;
    forEachChunk([&](const uint32_t i) {
        const auto pos = m_chunks[i]->getPosition();
        if (m_chunks[i]->isGpuBufferDirty() && areChunkAndNeighborsPresent(pos)) {
            chunks.emplace_back(*m_chunks[i]);
        }
    });
    return chunks;
}

VoxelData ChunkMap::getVoxel(const WorldCoordinate &coord) const {
    const auto cPos = coord.chunkPosition();
    const auto lPos = coord.localPosition();

    if (const auto chunk = tryGetChunkPtr(cPos)) {
        return chunk->getVoxel(lPos.x, lPos.y, lPos.z);
    }

    return VoxelData{};
}

bool ChunkMap::hasVoxel(const WorldCoordinate &coord) const {
    const auto cPos = coord.chunkPosition();
    const auto lPos = coord.localPosition();

    if (const auto chunk = tryGetChunkPtr(cPos)) {
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
    memset(m_chunkBitmap.data(), 0, m_chunkBitmap.size());
}

void ChunkMap::setChunkDirty(const glm::ivec3 &key) {
    if (auto* neighbor = tryGetChunkPtr(key)) {
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