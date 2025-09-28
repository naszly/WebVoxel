#pragma once

#include <vector>
#include <functional>
#include <webgpu/webgpu.h>
#include <glm/glm.hpp>

#include "application/graphics/Camera.h"
#include "application/world/World.h"
#include "application/graphics/types/ChunkVertexBuffer.h"
#include "application/types/VertexData.h"
#include "common/datastructures/HashMap.h"

class ChunkRenderManager {
    using ChunkRef = std::reference_wrapper<Chunk>;
public:
    ChunkRenderManager(const WGPUDevice &device, World& world) : m_device(device), m_world(world) {}
    ~ChunkRenderManager();

    [[nodiscard]] std::vector<ChunkVertexBuffer> getChunksToRender(const Camera& camera) const;

    [[nodiscard]] std::vector<ChunkVertexBuffer> getChunksToRender(const glm::vec3& cameraPosition,
                                                                   const glm::mat4& projView) const;

    void removeBuffersOfFarChunks(const Camera& camera);

    void updateChunkVertexBuffer(const std::vector<VertexData>& points, const glm::vec3 &position);

private:
    const WGPUDevice& m_device;
    World& m_world;
    HashMap<glm::ivec3, ChunkVertexBuffer> m_chunkVertexBuffers;

    [[nodiscard]] ChunkVertexBuffer createChunkVertexBuffer(const std::vector<VertexData>& points,
                                                            const glm::vec3 &position) const;

    [[nodiscard]] static bool isSphereInFrustum(const glm::vec3& center,
                                                float radius,
                                                glm::vec3 cameraPosition,
                                                glm::mat4 projView);
};
