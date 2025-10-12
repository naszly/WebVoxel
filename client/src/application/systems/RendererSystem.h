#pragma once

#include "System.h"

#include <glm/glm.hpp>
#include <memory>
#include <array>

#include "application/graphics/profiling/GpuTimestampProfiler.h"
#include "application/graphics/rendering/RenderTargets.h"
#include "application/graphics/resources/ShadowPass.h"
#include "application/graphics/resources/UniformsBuffer.h"
#include "application/graphics/ChunkRenderManager.h"
#include "application/types/VertexData.h"

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
    void exportTimestamps() const;

    void updateChunkVertexBuffer(const std::vector<VertexData>& vertexData, const glm::vec3& chunkPosition) const;

private:
    bool m_lighting = true;
    bool m_fog = true;
    bool m_pointLight = true;
    int m_sampleCount = 1;
    float m_timeAccumulator = 0.0f;

    struct Uniforms {
        glm::mat4 projectionViewMatrix;
        glm::mat4 inverseProjectionViewMatrix;
        glm::vec3 cameraPosition;
        float fov;
        glm::vec2 viewportSize;
        float nearPlane;
        float farPlane;
        glm::vec3 cameraDir;
        float time;
        glm::mat4 lightProjectionViewMatrixNear;
        glm::mat4 lightProjectionViewMatrixFar;
        glm::vec3 lightDirection;
        float padding{0.0f};
    };
    static_assert(sizeof(Uniforms) % 16 == 0, "Uniform buffer size must be a multiple of 16 bytes");

    std::unique_ptr<RenderTargets> m_renderTargets;
    unsigned int m_viewportWidth{}, m_viewportHeight{};

    WGPUQueue m_queue{};
    WGPURenderPipeline m_renderPipeline{};
    WGPUBuffer m_billboardVertexBuffer{};
    WGPUBuffer m_billboardIndexBuffer{};
    uint32_t m_billboardIndexCount{};
    std::unique_ptr<UniformsBuffer> m_uniformsBuffer;
    WGPUBindGroup m_uniformBindGroup{};

    std::unique_ptr<ChunkRenderManager> m_chunkRenderManager;

    GpuTimestampProfiler m_profiler{};

    WGPURenderPipeline m_fxaaPipeline{};
    WGPUBindGroup m_fxaaBindGroup{};
    WGPUSampler m_fxaaSampler{};
    WGPUBindGroupLayout m_fxaaBindGroupLayout{};

    enum class ShadowCascade : size_t { Near = 0, Far = 1, Count = 2 };
    std::array<std::unique_ptr<ShadowPass>, static_cast<size_t>(ShadowCascade::Count)> m_shadowCascades{};
    WGPUSampler m_shadowSampler{};

    void createPipelines();

    void createRenderPipeline();
    void initializeBuffers();
    void createFxaaPipeline();
    void createFxaaBindGroup();
    void updateUniformBuffer() const;

    void renderShadowPass(const WGPUCommandEncoder& encoder, const ShadowPass& shadowPass) const;
};