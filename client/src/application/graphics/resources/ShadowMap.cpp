#include "ShadowMap.h"

#include "application/graphics/pipeline/ShadowPipelineBuilder.h"

ShadowMap::ShadowMap(const WGPUDevice &device,
                     const WGPUBuffer &uniformBuffer,
                     const size_t chunkSize,
                     const uint32_t mapSize)
    : m_size(mapSize) {
    createResources(device);
    createPipeline(device, uniformBuffer, chunkSize);
}

ShadowMap::~ShadowMap() {
    releaseResources();
}

void ShadowMap::createResources(const WGPUDevice &device) {
    // Depth texture for the shadow map
    WGPUTextureDescriptor texDesc{};
    texDesc.dimension = WGPUTextureDimension_2D;
    texDesc.size.width = m_size;
    texDesc.size.height = m_size;
    texDesc.size.depthOrArrayLayers = 1;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.format = WGPUTextureFormat_Depth32Float;
    texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    m_depthTexture = wgpuDeviceCreateTexture(device, &texDesc);
    m_depthView = wgpuTextureCreateView(m_depthTexture, nullptr);

    // Comparison sampler for shadow lookups
    WGPUSamplerDescriptor samplerDesc{};
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.compare = WGPUCompareFunction_Less;
    samplerDesc.maxAnisotropy = 1;
    m_sampler = wgpuDeviceCreateSampler(device, &samplerDesc);
}

void ShadowMap::createPipeline(const WGPUDevice &device, const WGPUBuffer &uniformBuffer, size_t chunkSize) {
    auto artifacts = ShadowPipelineBuilder::build(device, uniformBuffer, chunkSize);
    m_pipeline = artifacts.pipeline;
    m_bindGroup = artifacts.bindGroup;
}

void ShadowMap::releaseResources() {
    if (m_bindGroup) { wgpuBindGroupRelease(m_bindGroup); m_bindGroup = nullptr; }
    if (m_pipeline) { wgpuRenderPipelineRelease(m_pipeline); m_pipeline = nullptr; }
    if (m_sampler) { wgpuSamplerRelease(m_sampler); m_sampler = nullptr; }
    if (m_depthView) { wgpuTextureViewRelease(m_depthView); m_depthView = nullptr; }
    if (m_depthTexture) { wgpuTextureRelease(m_depthTexture); m_depthTexture = nullptr; }
}

