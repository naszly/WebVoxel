#pragma once

#include "System.h"

#include <glm/glm.hpp>

class RendererSystem : public System {
public:
    void initialize(const Window&) override;
    void render() override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    RendererSystem() : System() {}

private:
    struct Uniforms {
        glm::mat4 projectionViewMatrix;
        glm::mat4 inverseProjectionViewMatrix;
        glm::vec3 cameraPosition;
        float fov;
        glm::vec2 viewportSize;
        float nearPlane;
        float farPlane;
    };

    std::shared_ptr<WebGPUContext> m_Context = nullptr;

    WGPUTexture depthTexture{};
    WGPUTextureView depthTextureView{};

    WGPUQueue m_Queue{};
    WGPURenderPipeline m_RenderPipeline{};
    WGPUBuffer vertexBuffer{};
    uint32_t vertexCount{};
    WGPUBuffer indexBuffer{};
    uint32_t indexCount{};
    WGPUBuffer instanceBuffer{};
    uint32_t instanceCount{};
    WGPUBuffer uniformBuffer{};
    Uniforms uniformData{};
    WGPUBindGroup uniformBindGroup{};

    void createRenderPipeline();
    void createDepthTexture();
    WGPUTextureView GetNextSurfaceTextureView();
    static std::string LoadShader(const char* filename);
    void InitializeBuffers();
};