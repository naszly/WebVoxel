#include "World.h"

ChunkNeighbours World::getChunkNeighbours(const glm::ivec3 &chunkPos) const {
    return ChunkNeighbours{
        .xMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 0, 0)),
        .xPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, 0, 0)),
        .yMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, -1, 0)),
        .yPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, 1, 0)),
        .zMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, 0, -1)),
        .zPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, 0, 1)),

        .xMinusYMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, -1, 0)),
        .xMinusYPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 1, 0)),
        .xMinusZMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 0, -1)),
        .xMinusZPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 0, 1)),
        .xPlusYMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, -1, 0)),
        .xPlusYPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, 1, 0)),
        .xPlusZMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, 0, -1)),
        .xPlusZPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, 0, 1)),
        .yMinusZMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, -1, -1)),
        .yMinusZPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, -1, 1)),
        .yPlusZMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, 1, -1)),
        .yPlusZPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, 1, 1)),

        .xMinusYMinusZMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, -1, -1)),
        .xMinusYMinusZPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, -1, 1)),
        .xMinusYPlusZMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 1, -1)),
        .xMinusYPlusZPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 1, 1)),
        .xPlusYMinusZMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, -1, -1)),
        .xPlusYMinusZPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, -1, 1)),
        .xPlusYPlusZMinus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, 1, -1)),
        .xPlusYPlusZPlus = m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, 1, 1))
    };
}

bool World::hasChunk(const glm::ivec3 &chunkPos) const {
    return m_Chunks.hasChunk(chunkPos);
}

void World::moveChunk(Chunk &chunk) {
    m_Chunks.moveChunk(chunk);
}

void World::removeChunk(const glm::ivec3 &chunkPos) {
    m_Chunks.removeChunk(chunkPos);
}

VoxelData World::getVoxel(const WorldCoordinate &coord) const {
    return m_Chunks.getVoxel(coord);
}

bool World::hasVoxel(const WorldCoordinate &coord) const {
    return m_Chunks.hasVoxel(coord);
}

void World::setVoxel(const WorldCoordinate &coord, const VoxelData voxel) {
    m_Chunks.setVoxel(coord, voxel);
}

void World::setVoxel(const WorldCoordinate &coord, const VoxelData voxel, const int64_t radius, const bool isSphere) {
    for (int64_t x = -radius; x <= radius; x++) {
        for (int64_t y = -radius; y <= radius; y++) {
            for (int64_t z = -radius; z <= radius; z++) {
                const auto pos = WorldCoordinate(coord.worldPosition() + glm::i64vec3(x, y, z));
                if (!isSphere || Utils::distance(pos.worldPosition(), coord.worldPosition()) <= radius) {
                    setVoxel(pos, voxel);
                }
            }
        }
    }
}

void World::removeVoxel(const WorldCoordinate &coord) {
    m_Chunks.removeVoxel(coord);
}

void World::removeVoxel(const WorldCoordinate &coord, const int64_t radius, const bool isSphere) {
    for (int64_t x = -radius; x <= radius; x++) {
        for (int64_t y = -radius; y <= radius; y++) {
            for (int64_t z = -radius; z <= radius; z++) {
                const auto pos = WorldCoordinate(coord.worldPosition() + glm::i64vec3(x, y, z));
                if (!isSphere || Utils::distance(pos.worldPosition(), coord.worldPosition()) <= radius) {
                    removeVoxel(pos);
                }
            }
        }
    }
}
