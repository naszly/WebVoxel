#include "World.h"

ChukNeighbours World::getExtendedChunkNeighbours(const glm::ivec3 &chunkPos) const {
    return {
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 0, 0)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(1, 0, 0)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(0, -1, 0)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(0, 1, 0)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(0, 0, -1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(0, 0, 1)),

        m_chunks.tryGetChunk(chunkPos + glm::ivec3(-1, -1, 0)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 1, 0)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 0, -1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 0, 1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(1, -1, 0)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(1, 1, 0)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(1, 0, -1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(1, 0, 1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(0, -1, -1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(0, -1, 1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(0, 1, -1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(0, 1, 1)),

        m_chunks.tryGetChunk(chunkPos + glm::ivec3(-1, -1, -1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(-1, -1, 1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 1, -1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 1, 1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(1, -1, -1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(1, -1, 1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(1, 1, -1)),
        m_chunks.tryGetChunk(chunkPos + glm::ivec3(1, 1, 1)),
    };
}

bool World::hasChunk(const glm::ivec3 &chunkPos) const {
    return m_chunks.hasChunk(chunkPos);
}

void World::createChunk(const glm::ivec3 &chunkPos) {
    m_chunks.createChunk(chunkPos);
}

void World::insertChunkByMove(Chunk &chunk) {
    m_chunks.insertChunkByMove(chunk);
}

Chunk World::extractChunkByMove(const glm::ivec3& chunkPos) {
    return m_chunks.extractChunkByMove(chunkPos);
}

VoxelData World::getVoxel(const WorldCoordinate &coord) const {
    return m_chunks.getVoxel(coord);
}

bool World::hasVoxel(const WorldCoordinate &coord) const {
    return m_chunks.hasVoxel(coord);
}

void World::setVoxel(const WorldCoordinate &coord, const VoxelData voxel) {
    m_chunks.setVoxel(coord, voxel);
}

void World::setVoxel(const WorldCoordinate &coord, const VoxelData voxel, const int64_t radius, const bool isSphere) {
    HashSet<Chunk*> chunks;

    for (int64_t x = -radius; x <= radius; x++) {
        for (int64_t y = -radius; y <= radius; y++) {
            for (int64_t z = -radius; z <= radius; z++) {
                const auto pos = WorldCoordinate(coord.worldPosition() + glm::i64vec3(x, y, z));
                if (!isSphere || Utils::distance(pos.worldPosition(), coord.worldPosition()) <= radius) {
                    Chunk* chunk = m_chunks.queueVoxelToSet(pos, voxel);
                    chunks.insert(chunk);
                }
            }
        }
    }

    for (const auto &chunk : chunks) {
        m_chunks.executeQueuedVoxelsToSet(chunk);
    }
}

void World::removeVoxel(const WorldCoordinate &coord) {
    setVoxel(coord, VoxelData{});
}

void World::removeVoxel(const WorldCoordinate &coord, const int64_t radius, const bool isSphere) {
    setVoxel(coord, VoxelData{}, radius, isSphere);
}
