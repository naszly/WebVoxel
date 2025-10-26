#include "VoxelVertexGenerator.h"

#include <vector>

#include "AmbientOcclusionComputer.h"
#include "PointLightPropagator.h"
#include "application/world/chunk/Chunk.h"

void VoxelVertexGenerator::generate(const ChunkNeighborhood& neighborChunks, std::vector<VertexData>& vertices) {
    const auto& centerChunk = neighborChunks.getCenterChunk();

    std::vector<Chunk::LightSource> lights;
    lights.reserve(16);
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                const auto& chunk = neighborChunks.getChunk(x, y, z);
                auto chunkLight = chunk->getLightSources(x - 1, y - 1, z - 1);
                lights.insert(lights.end(), chunkLight.begin(), chunkLight.end());
            }
        }
    }

    const auto& lightMap = PointLightPropagator::compute(neighborChunks, lights);

    for (uint32_t x = Chunk::WIDTH; x < 2 * Chunk::WIDTH; ++x) {
        for (uint32_t y = Chunk::WIDTH; y < 2 * Chunk::WIDTH; ++y) {
            for (uint32_t z = Chunk::WIDTH; z < 2 * Chunk::WIDTH; ++z) {
                if (neighborChunks.hasVoxelAt(x, y, z)) {
                    const bool nx = neighborChunks.hasVoxelAt(x - 1, y, z);
                    const bool px = neighborChunks.hasVoxelAt(x + 1, y, z);
                    const bool ny = neighborChunks.hasVoxelAt(x, y - 1, z);
                    const bool py = neighborChunks.hasVoxelAt(x, y + 1, z);
                    const bool nz = neighborChunks.hasVoxelAt(x, y, z - 1);
                    const bool pz = neighborChunks.hasVoxelAt(x, y, z + 1);
                    const bool surrounded = nx & px & ny & py & nz & pz;
                    if (!surrounded) {
                        const uint8_t vx = static_cast<uint8_t>(x - Chunk::WIDTH);
                        const uint8_t vy = static_cast<uint8_t>(y - Chunk::WIDTH);
                        const uint8_t vz = static_cast<uint8_t>(z - Chunk::WIDTH);
                        if (vx < Chunk::WIDTH && vy < Chunk::WIDTH && vz < Chunk::WIDTH) {
                            const auto& voxel = centerChunk->getVoxel(vx, vy, vz);

                            const auto ambientOcclusion = AmbientOcclusionComputer::compute(neighborChunks, x, y, z);

                            const auto& faceNxLightIntensity = lightMap.getLightInfo(x - 1, y, z);
                            const auto& facePxLightIntensity = lightMap.getLightInfo(x + 1, y, z);
                            const auto& faceNyLightIntensity = lightMap.getLightInfo(x, y - 1, z);
                            const auto& facePyLightIntensity = lightMap.getLightInfo(x, y + 1, z);
                            const auto& faceNzLightIntensity = lightMap.getLightInfo(x, y, z - 1);
                            const auto& facePzLightIntensity = lightMap.getLightInfo(x, y, z + 1);
                            const auto voxelLight = PackedLight(
                                faceNxLightIntensity, facePxLightIntensity,
                                faceNyLightIntensity, facePyLightIntensity,
                                faceNzLightIntensity, facePzLightIntensity);

                            VertexData voxelData = { vx, vy, vz, 1, voxel, ambientOcclusion, voxelLight };
                            vertices.emplace_back(voxelData);
                        }
                    }
                }
            }
        }
    }
}

void VoxelVertexGenerator::generateDownsample2(const ChunkNeighborhood& neighborChunks,
                                               std::vector<VertexData>& vertices) {
    generateDownsample<2>(neighborChunks, vertices);
}

void VoxelVertexGenerator::generateDownsample4(const ChunkNeighborhood& neighborChunks,
                                               std::vector<VertexData>& vertices) {
    generateDownsample<4>(neighborChunks, vertices);
}

void VoxelVertexGenerator::generateDownsample8(const ChunkNeighborhood& neighborChunks,
                                               std::vector<VertexData>& vertices) {
    generateDownsample<8>(neighborChunks, vertices);
}

template <size_t BlockSize>
void VoxelVertexGenerator::generateDownsample(const ChunkNeighborhood& neighborChunks,
                                              std::vector<VertexData>& vertices) {
    constexpr uint32_t blockSize = BlockSize;

    constexpr uint32_t lowResChunkWidth = Chunk::WIDTH / blockSize;
    constexpr uint32_t bitmapWidth = lowResChunkWidth * 3;
    constexpr uint32_t bitmapWidth2 = bitmapWidth * bitmapWidth;
    Bitmap<bitmapWidth * bitmapWidth * bitmapWidth> bitmap;

    auto testBitmap = [&](const uint32_t x, const uint32_t y, const uint32_t z) -> bool {
        const uint32_t index = x * bitmapWidth2 + y * bitmapWidth + z;
        return bitmap.test(index);
    };

    auto setBitmap = [&](const uint32_t x, const uint32_t y, const uint32_t z) {
        const uint32_t index = x * bitmapWidth2 + y * bitmapWidth + z;
        bitmap.set(index);
    };

    auto blockMeetsThreshold = [&](const uint32_t bx, const uint32_t by, const uint32_t bz) -> bool {
        for (uint32_t dx = 0; dx < blockSize; ++dx) {
            for (uint32_t dy = 0; dy < blockSize; ++dy) {
                for (uint32_t dz = 0; dz < blockSize; ++dz) {
                    if (neighborChunks.hasVoxelAt(bx + dx, by + dy, bz + dz)) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    const uint32_t xStart = Chunk::WIDTH - blockSize;
    const uint32_t xEnd = 2 * Chunk::WIDTH + blockSize;

    for (uint32_t x = xStart; x < xEnd; x += blockSize) {
        for (uint32_t y = xStart; y < xEnd; y += blockSize) {
            for (uint32_t z = xStart; z < xEnd; z += blockSize) {
                if (blockMeetsThreshold(x, y, z)) {
                    const uint32_t bx = x / blockSize;
                    const uint32_t by = y / blockSize;
                    const uint32_t bz = z / blockSize;
                    setBitmap(bx, by, bz);
                }
            }
        }
    }

    auto isVisibleVoxel = [&](const uint32_t cx, const uint32_t cy, const uint32_t cz) -> bool {
        const uint32_t x = cx + Chunk::WIDTH;
        const uint32_t y = cy + Chunk::WIDTH;
        const uint32_t z = cz + Chunk::WIDTH;
        const bool nx = neighborChunks.hasVoxelAt(x-1, y, z);
        const bool px = neighborChunks.hasVoxelAt(x+1, y, z);
        const bool ny = neighborChunks.hasVoxelAt(x, y-1, z);
        const bool py = neighborChunks.hasVoxelAt(x, y+1, z);
        const bool nz = neighborChunks.hasVoxelAt(x, y, z-1);
        const bool pz = neighborChunks.hasVoxelAt(x, y, z+1);
        return neighborChunks.hasVoxelAt(x, y, z) && !(nx & px & ny & py & nz & pz);
    };

    for (uint32_t x = lowResChunkWidth; x < 2 * lowResChunkWidth; ++x) {
        for (uint32_t y = lowResChunkWidth; y < 2 * lowResChunkWidth; ++y) {
            for (uint32_t z = lowResChunkWidth; z < 2 * lowResChunkWidth; ++z) {
                if (testBitmap(x, y, z)) {
                    const bool nx = testBitmap(x - 1, y, z);
                    const bool px = testBitmap(x + 1, y, z);
                    const bool ny = testBitmap(x, y - 1, z);
                    const bool py = testBitmap(x, y + 1, z);
                    const bool nz = testBitmap(x, y, z - 1);
                    const bool pz = testBitmap(x, y, z + 1);
                    const bool surrounded = nx & px & ny & py & nz & pz;
                    if (!surrounded) {
                        const auto vx = static_cast<uint8_t>((x - lowResChunkWidth) * blockSize);
                        const auto vy = static_cast<uint8_t>((y - lowResChunkWidth) * blockSize);
                        const auto vz = static_cast<uint8_t>((z - lowResChunkWidth) * blockSize);

                        thread_local HashMap<uint32_t, uint32_t> voxelCounts;
                        voxelCounts.clear();
                        auto& centerChunk = *neighborChunks.getCenterChunk();
                        for (uint32_t dx = 0; dx < blockSize; ++dx) {
                            for (uint32_t dy = 0; dy < blockSize; ++dy) {
                                for (uint32_t dz = 0; dz < blockSize; ++dz) {
                                    if (isVisibleVoxel(vx + dx, vy + dy, vz + dz)) {
                                        VoxelData voxel = centerChunk.getVoxel(vx + dx, vy + dy, vz + dz);
                                        ++voxelCounts[static_cast<uint32_t>(voxel)];
                                    }
                                }
                            }
                        }

                        uint32_t maxCount = 0;
                        VoxelData dominantVoxel;
                        for (const auto& [voxel, count] : voxelCounts) {
                            if (count > maxCount) {
                                maxCount = count;
                                dominantVoxel = static_cast<VoxelData>(voxel);
                            }
                        }
                        VertexData voxelData = {
                            vx, vy, vz, blockSize,
                            dominantVoxel, AmbientOcclusion::None, {}
                        };
                        vertices.emplace_back(voxelData);
                    }
                }
            }
        }
    }
}
