#include "World.h"

void World::generate() {
    for (int i = 0; i < CHUNKS; i++) {
        for (int j = 0; j < CHUNKS; j++) {
            for (int k = 0; k < CHUNKS; k++) {
                if ((i + j) % 2 == 0 && (j + k) % 2 == 0 && (i + k) % 2 == 0) {
                    m_Chunks.getChunk(glm::ivec3(i, j, k)).generate();
                }
            }
        }
    }
}

std::vector<ChunkVertexBuffer> World::getChunkVertexBuffers() {
    Timer timer("World::getChunkVertexBuffers");
    auto chunks = m_Chunks.getChunks();

    std::vector<ChunkVertexBuffer> buffers(chunks.size());

    std::ranges::transform(chunks, buffers.begin(), [&](Chunk &chunk) {
        const auto chunkPos = chunk.getPosition();
        auto getChunkNeighbours = [&] {
            return this->getChunkNeighbours(chunkPos);
        };

        return chunk.getVertexBuffer(getChunkNeighbours);
    });

    return buffers;
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

ChunkNeighbours World::getChunkNeighbours(const glm::ivec3 &chunkPos) {
    return ChunkNeighbours{
        m_Chunks.tryGetChunk(chunkPos + glm::ivec3(-1, 0, 0)),
        m_Chunks.tryGetChunk(chunkPos + glm::ivec3(1, 0, 0)),
        m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, -1, 0)),
        m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, 1, 0)),
        m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, 0, -1)),
        m_Chunks.tryGetChunk(chunkPos + glm::ivec3(0, 0, 1))
    };
}
