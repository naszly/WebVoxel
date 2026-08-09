#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>

class ShadowMap {
public:
    ShadowMap(const WGPUDevice& device,
              const WGPUBuffer& uniformBuffer,
              size_t chunkSize,
              uint32_t mapSize);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    [[nodiscard]] WGPUTextureView getDepthView() const { return m_depthView; }
    [[nodiscard]] WGPURenderPipeline getPipeline() const { return m_pipeline; }
    [[nodiscard]] WGPURenderPipeline getRemotePlayerPipeline() const { return m_remotePlayerPipeline; }
    [[nodiscard]] WGPUBindGroup getBindGroup() const { return m_bindGroup; }
    [[nodiscard]] uint32_t getSize() const { return m_size; }

private:
    void createResources(const WGPUDevice& device);
    void createPipeline(const WGPUDevice& device, const WGPUBuffer& uniformBuffer, size_t chunkSize);
    void releaseResources();

    uint32_t m_size{4092};

    WGPUTexture m_depthTexture{};
    WGPUTextureView m_depthView{};
    WGPURenderPipeline m_pipeline{};
    WGPURenderPipeline m_remotePlayerPipeline{};
    WGPUBindGroup m_bindGroup{};
};
