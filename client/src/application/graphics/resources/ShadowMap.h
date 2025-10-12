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
    [[nodiscard]] WGPUSampler getSampler() const { return m_sampler; }
    [[nodiscard]] WGPURenderPipeline getPipeline() const { return m_pipeline; }
    [[nodiscard]] WGPUBindGroup getBindGroup() const { return m_bindGroup; }
    [[nodiscard]] uint32_t getSize() const { return m_size; }

private:
    void createResources(const WGPUDevice& device);
    void createPipeline(const WGPUDevice& device, const WGPUBuffer& uniformBuffer, size_t chunkSize);
    void releaseResources();

    uint32_t m_size{4092};

    WGPUTexture m_depthTexture{};
    WGPUTextureView m_depthView{};
    WGPUSampler m_sampler{};
    WGPURenderPipeline m_pipeline{};
    WGPUBindGroup m_bindGroup{};
};

