#include "World.h"

void World::generate() {
    for (int i = 0; i < CHUNKS; i++) {
        for (int j = 0; j < CHUNKS; j++) {
            for (int k = 0; k < CHUNKS; k++) {
                if ((i + j) % 2 == 0 && (j + k) % 2 == 0 && (i + k) % 2 == 0) {
                    m_Chunks.getChunk(i, j, k).generate(i, j, k);
                }
            }
        }
    }
}

std::vector<std::pair<glm::ivec3, Chunk &>> World::getChunks() {
    return m_Chunks.getChunks();
}

VoxelData World::getVoxel(const WorldCoordinate &coord) {
    return m_Chunks.getVoxel(coord);
}

bool World::hasVoxel(const WorldCoordinate &coord) {
    return !m_Chunks.getVoxel(coord).isEmpty();
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
