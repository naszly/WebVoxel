#pragma once

#include <webgpu/webgpu.h>
#include "core/webgpu/WebGPUContext.h"

class RenderTargets {
public:
    explicit RenderTargets(const WebGpuContext& context) : m_context(context) {}
    ~RenderTargets();

    // Create or recreate depth and optional MSAA color targets
    void configure(unsigned int width, unsigned int height, int sampleCount, WGPUTextureFormat colorFormat);

    [[nodiscard]] bool isMultisampled() const { return m_sampleCount > 1; }

    [[nodiscard]] WGPUTextureView getDepthView() const { return m_depthTextureView; }
    [[nodiscard]] WGPUTextureView getMsaaColorView() const { return m_multisampleColorTextureView; }

    [[nodiscard]] WGPUTextureView colorAttachmentViewFor(const WGPUTextureView& swapchainView) const {
        return isMultisampled() ? m_multisampleColorTextureView : swapchainView;
    }
    [[nodiscard]] WGPUTextureView resolveTargetFor(const WGPUTextureView& swapchainView) const {
        return isMultisampled() ? swapchainView : nullptr;
    }

private:
    void destroy();

    const WebGpuContext& m_context;

    int m_sampleCount = 1;
    unsigned int m_width = 0, m_height = 0;
    WGPUTextureFormat m_colorFormat = WGPUTextureFormat_Undefined;

    WGPUTexture m_depthTexture{};
    WGPUTextureView m_depthTextureView{};
    WGPUTexture m_multisampleColorTexture{};
    WGPUTextureView m_multisampleColorTextureView{};
};

