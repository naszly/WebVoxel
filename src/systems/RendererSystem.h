#pragma once

#include "System.h"

#include <glm/glm.hpp>

class RendererSystem : public System {
public:
    void initialize() override;
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
    static_assert(sizeof(Uniforms) % 16 == 0);

    struct DynamicUniforms {
        glm::vec4 positionOffset;
    };
    static_assert(sizeof(DynamicUniforms) % 16 == 0);

    WGPUTexture depthTexture{};
    WGPUTextureView depthTextureView{};
    unsigned int  m_viewportWidth{}, m_viewportHeight{};

    WGPUQueue m_Queue{};
    WGPURenderPipeline m_RenderPipeline{};
    WGPUBuffer vertexBuffer{};
    uint32_t vertexCount{};
    WGPUBuffer indexBuffer{};
    uint32_t indexCount{};
    WGPUBuffer uniformBuffer{};
    Uniforms uniformData{};
    WGPUBuffer dynamicUniformBuffer{};
    DynamicUniforms dynamicUniformData{};
    WGPUBindGroup uniformBindGroup{};

    void createRenderPipeline();
    void createDepthTexture();
    WGPUTextureView GetNextSurfaceTextureView();
    static std::string LoadShader(const char* filename);
    void InitializeBuffers();
};