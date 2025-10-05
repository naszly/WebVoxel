#include "RenderTargets.h"

#include "common/Log.h"

RenderTargets::~RenderTargets() {
    destroy();
}

void RenderTargets::destroy() {
    if (m_depthTextureView) {
        wgpuTextureViewRelease(m_depthTextureView);
        m_depthTextureView = nullptr;
    }
    if (m_depthTexture) {
        wgpuTextureDestroy(m_depthTexture);
        wgpuTextureRelease(m_depthTexture);
        m_depthTexture = nullptr;
    }
    if (m_sceneColorTextureView) {
        wgpuTextureViewRelease(m_sceneColorTextureView);
        m_sceneColorTextureView = nullptr;
    }
    if (m_sceneColorTexture) {
        wgpuTextureDestroy(m_sceneColorTexture);
        wgpuTextureRelease(m_sceneColorTexture);
        m_sceneColorTexture = nullptr;
    }
}

void RenderTargets::configure(unsigned int width, unsigned int height, WGPUTextureFormat colorFormat) {
    const bool dimsSame = (m_width == width && m_height == height);
    const bool fmtSame = (m_colorFormat == colorFormat);

    if (dimsSame && fmtSame && m_depthTextureView) {
        return;
    }

    destroy();

    m_width = width;
    m_height = height;
    m_colorFormat = colorFormat;

    const auto& device = m_context.getDevice();

    // Create the depth texture
    WGPUTextureDescriptor depthTextureDesc = {};
    depthTextureDesc.size.width = m_width;
    depthTextureDesc.size.height = m_height;
    depthTextureDesc.size.depthOrArrayLayers = 1;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 1;
    depthTextureDesc.dimension = WGPUTextureDimension_2D;
    depthTextureDesc.format = WGPUTextureFormat_Depth24PlusStencil8;
    depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;

    m_depthTexture = wgpuDeviceCreateTexture(device, &depthTextureDesc);
    if (!m_depthTexture) {
        LogApp::error("RenderTargets: Failed to create depth texture");
        return;
    }

    WGPUTextureViewDescriptor depthViewDesc = {};
    depthViewDesc.format = WGPUTextureFormat_Depth24PlusStencil8;
    depthViewDesc.dimension = WGPUTextureViewDimension_2D;
    depthViewDesc.baseMipLevel = 0;
    depthViewDesc.mipLevelCount = 1;
    depthViewDesc.baseArrayLayer = 0;
    depthViewDesc.arrayLayerCount = 1;
    depthViewDesc.aspect = WGPUTextureAspect_All;

    m_depthTextureView = wgpuTextureCreateView(m_depthTexture, &depthViewDesc);
    if (!m_depthTextureView) {
        LogApp::error("RenderTargets: Failed to create depth texture view");
        wgpuTextureRelease(m_depthTexture);
        m_depthTexture = nullptr;
        return;
    }

    // Create the scene color texture for post-processing (FXAA)
    WGPUTextureDescriptor sceneColorDesc = {};
    sceneColorDesc.size.width = m_width;
    sceneColorDesc.size.height = m_height;
    sceneColorDesc.size.depthOrArrayLayers = 1;
    sceneColorDesc.mipLevelCount = 1;
    sceneColorDesc.sampleCount = 1;
    sceneColorDesc.dimension = WGPUTextureDimension_2D;
    sceneColorDesc.format = m_colorFormat;
    sceneColorDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;

    m_sceneColorTexture = wgpuDeviceCreateTexture(device, &sceneColorDesc);
    if (!m_sceneColorTexture) {
        LogApp::error("RenderTargets: Failed to create scene color texture");
        return;
    }

    WGPUTextureViewDescriptor sceneColorViewDesc = {};
    sceneColorViewDesc.format = m_colorFormat;
    sceneColorViewDesc.dimension = WGPUTextureViewDimension_2D;
    sceneColorViewDesc.baseMipLevel = 0;
    sceneColorViewDesc.mipLevelCount = 1;
    sceneColorViewDesc.baseArrayLayer = 0;
    sceneColorViewDesc.arrayLayerCount = 1;
    sceneColorViewDesc.aspect = WGPUTextureAspect_All;

    m_sceneColorTextureView = wgpuTextureCreateView(m_sceneColorTexture, &sceneColorViewDesc);
    if (!m_sceneColorTextureView) {
        LogApp::error("RenderTargets: Failed to create scene color texture view");
        wgpuTextureRelease(m_sceneColorTexture);
        m_sceneColorTexture = nullptr;
        return;
    }
}
