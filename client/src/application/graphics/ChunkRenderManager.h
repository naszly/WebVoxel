#pragma once

#include <vector>
#include <functional>
#include <webgpu/webgpu.h>
#include <glm/glm.hpp>

#include "application/graphics/Camera.h"
#include "application/world/World.h"
#include "application/world/chunk/ChunkNeighborhood.h"
#include "application/graphics/types/ChunkVertexBuffer.h"
#include "common/datastructures/HashMap.h"

class ChunkRenderManager {
    using ChunkRef = std::reference_wrapper<Chunk>;
public:
    ChunkRenderManager(const WGPUDevice &device, World& world) : m_device(device), m_world(world) {}
    ~ChunkRenderManager();

    std::vector<ChunkVertexBuffer> getChunksToRender(const Camera& camera) const;

    void update(const Camera& camera);

private:
    const WGPUDevice& m_device;
    World& m_world;
    HashMap<glm::ivec3, ChunkVertexBuffer> m_chunkVertexBuffers;

    void updateDirtyChunks(const glm::ivec3& playerChunk);

    void removeBuffersOfFarChunks(const glm::ivec3& playerChunk);

    std::vector<ChunkRef> collectDirtyChunks(const glm::ivec3& playerChunk) const;

    void processDirtyChunks(const std::vector<ChunkRef>& dirtyChunks);

    ChunkVertexBuffer createChunkVertexBuffer(const ChunkNeighborhood& neighborChunks) const;
};
