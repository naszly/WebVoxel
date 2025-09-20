#include "World.h"
#include <array>

ChunkNeighborhoodPtrs World::getChunkNeighborhoodPtrs(const glm::ivec3 &chunkPos) const {
    std::array<std::array<std::array<const Chunk*, 3>, 3>, 3> neighbours{};
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                glm::ivec3 offset(x - 1, y - 1, z - 1);
                neighbours[x][y][z] = m_chunks.tryGetChunkPtr(chunkPos + offset);
            }
        }
    }
    return ChunkNeighborhoodPtrs(neighbours);
}

ChunkNeighborhood World::getChunkNeighborhood(const glm::ivec3& chunkPos) const {
    std::array<std::array<std::array<std::optional<Chunk>, 3>, 3>, 3> neighbours{};
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                glm::ivec3 offset(x - 1, y - 1, z - 1);
                if (const auto& chunk = m_chunks.tryGetChunk(chunkPos + offset)) {
                    neighbours[x][y][z] = chunk;
                }
            }
        }
    }
    return ChunkNeighborhood(std::move(neighbours));
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
