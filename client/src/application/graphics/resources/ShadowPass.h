#pragma once

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <memory>

#include "application/graphics/resources/UniformsBuffer.h"
#include "application/graphics/resources/ShadowMap.h"

class ShadowPass {
public:
    struct Uniforms {
        glm::mat4 projectionViewMatrix;
        glm::mat4 inverseProjectionViewMatrix;
        glm::vec3 cameraPosition;
        float time;
        glm::vec2 viewportSize;
        float nearPlane;
        float farPlane;
        glm::vec3 cameraDir;
        float padding{0.0f};
    };
    static_assert(sizeof(Uniforms) % 16 == 0, "Shadow uniform buffer size must be a multiple of 16 bytes");

    ShadowPass(const WGPUDevice& device, size_t chunkSize, const uint32_t mapSize)
        : m_size(mapSize) {
        m_uniforms = UniformsBuffer::make<Uniforms>(device);
        m_map = std::make_unique<ShadowMap>(device, m_uniforms->get(), chunkSize, m_size);
    }

    void update(const WGPUQueue& queue,
                const glm::mat4& lightProjectionView,
                const glm::vec3& cameraPosition,
                const glm::vec2& viewportSize,
                const float nearPlane,
                const float farPlane,
                const glm::vec3& lightDir,
                const float time) {

        m_lightProjectionView = lightProjectionView;

        const Uniforms u{
            .projectionViewMatrix = m_lightProjectionView,
            .inverseProjectionViewMatrix = glm::inverse(m_lightProjectionView),
            .cameraPosition = cameraPosition,
            .time = time,
            .viewportSize = viewportSize,
            .nearPlane = nearPlane,
            .farPlane = farPlane,
            .cameraDir = lightDir,
        };

        m_uniforms->write(queue, u);
    }

    static glm::mat4 computeDirectionalLightProjectionView(const double orthoHalf,
                                                           const double nearPlane,
                                                           const double farPlane,
                                                           const glm::dvec3& lightDir) {
        const glm::mat4 lightView = glm::lookAt(
            -glm::normalize(lightDir),
            glm::dvec3(0),
            glm::dvec3(0, 1, 0));

        const glm::mat4 lightProj = glm::ortho(
            -orthoHalf, orthoHalf,
            -orthoHalf, orthoHalf,
            nearPlane, farPlane);

        return lightProj * lightView;
    }

    [[nodiscard]] WGPUTextureView getDepthView() const { return m_map->getDepthView(); }
    [[nodiscard]] WGPURenderPipeline getPipeline() const { return m_map->getPipeline(); }
    [[nodiscard]] WGPUBindGroup getBindGroup() const { return m_map->getBindGroup(); }

    [[nodiscard]] uint32_t getSize() const { return m_size; }
    [[nodiscard]] const glm::mat4& getLightProjectionView() const { return m_lightProjectionView; }

private:
    std::unique_ptr<UniformsBuffer> m_uniforms;
    std::unique_ptr<ShadowMap> m_map;
    uint32_t m_size{4092};
    glm::mat4 m_lightProjectionView{1.0f};
};

