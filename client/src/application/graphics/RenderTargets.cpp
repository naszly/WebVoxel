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
    if (m_multisampleColorTextureView) {
        wgpuTextureViewRelease(m_multisampleColorTextureView);
        m_multisampleColorTextureView = nullptr;
    }
    if (m_multisampleColorTexture) {
        wgpuTextureDestroy(m_multisampleColorTexture);
        wgpuTextureRelease(m_multisampleColorTexture);
        m_multisampleColorTexture = nullptr;
    }
}

void RenderTargets::configure(unsigned int width, unsigned int height, int sampleCount, WGPUTextureFormat colorFormat) {
    const bool dimsSame = (m_width == width && m_height == height);
    const bool msaaSame = (m_sampleCount == sampleCount);
    const bool fmtSame = (m_colorFormat == colorFormat);

    if (dimsSame && msaaSame && fmtSame && m_depthTextureView) {
        return;
    }

    destroy();

    m_width = width;
    m_height = height;
    m_sampleCount = sampleCount;
    m_colorFormat = colorFormat;

    const auto& device = m_context.getDevice();

    // Create the depth texture
    WGPUTextureDescriptor depthTextureDesc = {};
    depthTextureDesc.size.width = m_width;
    depthTextureDesc.size.height = m_height;
    depthTextureDesc.size.depthOrArrayLayers = 1;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = m_sampleCount;
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

    // Create the MSAA color texture only if multisampling is enabled
    if (m_sampleCount > 1) {
        WGPUTextureDescriptor colorTextureDesc = {};
        colorTextureDesc.size.width = m_width;
        colorTextureDesc.size.height = m_height;
        colorTextureDesc.size.depthOrArrayLayers = 1;
        colorTextureDesc.mipLevelCount = 1;
        colorTextureDesc.sampleCount = m_sampleCount;
        colorTextureDesc.dimension = WGPUTextureDimension_2D;
        colorTextureDesc.format = m_colorFormat;
        colorTextureDesc.usage = WGPUTextureUsage_RenderAttachment;

        m_multisampleColorTexture = wgpuDeviceCreateTexture(device, &colorTextureDesc);
        if (!m_multisampleColorTexture) {
            LogApp::error("RenderTargets: Failed to create MSAA color texture");
            return;
        }

        WGPUTextureViewDescriptor colorViewDesc = {};
        colorViewDesc.format = m_colorFormat;
        colorViewDesc.dimension = WGPUTextureViewDimension_2D;
        colorViewDesc.baseMipLevel = 0;
        colorViewDesc.mipLevelCount = 1;
        colorViewDesc.baseArrayLayer = 0;
        colorViewDesc.arrayLayerCount = 1;
        colorViewDesc.aspect = WGPUTextureAspect_All;

        m_multisampleColorTextureView = wgpuTextureCreateView(m_multisampleColorTexture, &colorViewDesc);
        if (!m_multisampleColorTextureView) {
            LogApp::error("RenderTargets: Failed to create MSAA color texture view");
            wgpuTextureRelease(m_multisampleColorTexture);
            m_multisampleColorTexture = nullptr;
        }
    }
}
