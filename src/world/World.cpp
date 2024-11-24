#include "World.h"

void World::update(const glm::vec3 playerPosition) {
    static constexpr int64_t maxRadius = 4;
    static int64_t currentRadius = 0;

    auto closestMissingChunk = WorldCoordinate(glm::i64vec3(0));
    bool foundMissingChunk = false;
    for (int64_t x = -currentRadius; x <= currentRadius; x++) {
        for (int64_t y = -currentRadius; y <= currentRadius; y++) {
            for (int64_t z = -currentRadius; z <= currentRadius; z++) {
                const auto pos = WorldCoordinate(glm::i64vec3(playerPosition) + glm::i64vec3(x * Chunk::SIZE, y * Chunk::SIZE, z * Chunk::SIZE));

                if (!m_Chunks.hasChunk(pos.chunkPosition())) {
                    if (!foundMissingChunk
                        || Utils::distance(pos.worldPosition(), playerPosition) < Utils::distance(closestMissingChunk.worldPosition(), playerPosition)) {
                        closestMissingChunk = pos;
                        foundMissingChunk = true;
                    }
                }
            }
        }
    }

    if (foundMissingChunk) {
        Chunk& chunk = m_Chunks.getChunk(closestMissingChunk.chunkPosition());
        if (chunk.getPosition().y < 0) {
            chunk.generate();
        }
    }

    if (currentRadius < maxRadius) {
        currentRadius++;
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
