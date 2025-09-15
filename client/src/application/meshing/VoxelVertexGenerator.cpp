#include "VoxelVertexGenerator.h"

#include <vector>

#include "AmbientOcclusionComputer.h"
#include "PointLightPropagator.h"
#include "application/world/chunk/Chunk.h"

void VoxelVertexGenerator::generate(const ChunkNeighborhood& neighborChunks, std::vector<VertexData>& vertices) {
    const auto& centerChunk = *neighborChunks.getCenterChunk();

    std::vector<Chunk::LightSource> lights;
    lights.reserve(16);
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                const auto& chunk = *neighborChunks.getChunk(x, y, z);
                auto chunkLight = chunk.getLightSources(x - 1, y - 1, z - 1);
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
                            const auto& voxel = centerChunk.getVoxel(vx, vy, vz);

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
