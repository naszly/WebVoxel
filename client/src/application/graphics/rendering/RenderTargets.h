#pragma once

#include <webgpu/webgpu.h>
#include "core/webgpu/WebGPUContext.h"

class RenderTargets {
public:
    explicit RenderTargets(const WebGpuContext& context) : m_context(context) {}
    ~RenderTargets();

    void configure(unsigned int width, unsigned int height, WGPUTextureFormat colorFormat);

    [[nodiscard]] WGPUTextureView getDepthView() const { return m_depthTextureView; }
    [[nodiscard]] WGPUTextureView getSceneColorView() const { return m_sceneColorTextureView; }
    [[nodiscard]] WGPUTexture getSceneColorTexture() const { return m_sceneColorTexture; }

private:
    void destroy();

    const WebGpuContext& m_context;

    unsigned int m_width = 0, m_height = 0;
    WGPUTextureFormat m_colorFormat = WGPUTextureFormat_Undefined;

    WGPUTexture m_depthTexture{};
    WGPUTextureView m_depthTextureView{};
    WGPUTexture m_sceneColorTexture{};
    WGPUTextureView m_sceneColorTextureView{};
};
