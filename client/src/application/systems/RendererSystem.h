#pragma once

#include "System.h"

#include <glm/glm.hpp>
#include <memory>

#include "common/datastructures/HashMap.h"
#include "../graphics/VertexData.h"
#include "application/domain/BlockLightInfo.h"
#include "application/graphics/GpuTimestampProfiler.h"
#include "application/graphics/RenderTargets.h"
#include "application/graphics/UniformsBuffer.h"

class RendererSystem final : public System {
public:
    void initialize() override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    explicit RendererSystem() : System() {}

    [[nodiscard]] bool getLighting() const {
        return m_lighting;
    }

    void setLighting(bool lighting);

    [[nodiscard]] bool getFog() const {
        return m_fog;
    }

    void setFog(bool fog);

    [[nodiscard]] bool getPointLight() const {
        return m_pointLight;
    }

    void setPointLight(bool pointLight);

    [[nodiscard]] bool getMultisampling() const {
        return m_sampleCount > 1;
    }

    void setMultisampling(bool multisampling);

    void exportTimestamps() const;

private:
    bool m_lighting = true;
    bool m_fog = true;
    bool m_pointLight = true;
    int m_sampleCount = 4;

    struct Uniforms {
        glm::mat4 projectionViewMatrix;
        glm::mat4 inverseProjectionViewMatrix;
        glm::vec3 cameraPosition;
        float fov;
        glm::vec2 viewportSize;
        float nearPlane;
        float farPlane;
        float time;
        glm::vec3 padding;
    };
    static_assert(sizeof(Uniforms) % 16 == 0);

    std::unique_ptr<RenderTargets> m_renderTargets;
    unsigned int m_viewportWidth{}, m_viewportHeight{};

    WGPUQueue m_queue{};
    WGPURenderPipeline m_renderPipeline{};
    WGPUBuffer m_billboardVertexBuffer{};
    WGPUBuffer m_billboardIndexBuffer{};
    uint32_t m_billboardIndexCount{};
    std::unique_ptr<UniformsBuffer> m_uniformsBuffer;
    Uniforms m_uniformData{};
    WGPUBindGroup m_uniformBindGroup{};

    struct ChunkVertexBuffer {
        WGPUBuffer buffer{nullptr};
        size_t vertexCount{0};
    };

    HashMap<glm::ivec3, ChunkVertexBuffer> m_chunkVertexBuffers;

    GpuTimestampProfiler m_profiler{};

    void createRenderPipeline();
    void initializeBuffers();

    auto getChunksToRender(const Camera& camera);

    std::vector<std::reference_wrapper<Chunk>> collectDirtyChunks(World& world, const glm::ivec3& playerChunk) const;
    void processDirtyChunks(const World& world, const std::vector<std::reference_wrapper<Chunk>>& dirtyChunks);
    void removeFarChunkBuffers(const glm::ivec3& playerChunk);


    [[nodiscard]] static bool hasAllNeighbours(const World &world, const Chunk& chunk);

    [[nodiscard]] ChunkVertexBuffer createChunkVertexBuffer(const ChunkNeighborhood& neighborChunks) const;
};