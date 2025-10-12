#include "RendererSystem.h"

#include "application/Application.h"
#include "application/graphics/pipeline/FxaaPipelineBuilder.h"
#include "application/graphics/pipeline/PipelineBuilder.h"
#include "application/graphics/pipeline/ShadowPipelineBuilder.h"
#include "core/events/ApplicationEvent.h"
#include "common/Log.h"

void RendererSystem::initialize() {
    LogApp::info("RendererSystem::initialize");

    auto& gpuContext = getWebGpuContext();
    auto& device = gpuContext.getDevice();

    m_queue = wgpuDeviceGetQueue(device);

    m_viewportWidth = getWebGpuSurface().getWidth();
    m_viewportHeight = getWebGpuSurface().getHeight();

    getCamera().setAspect(static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight));

    initializeBuffers();

    m_uniformsBuffer = UniformsBuffer::make<Uniforms>(device);
    m_shadowUniformsBuffer = UniformsBuffer::make<Uniforms>(device);

    m_renderTargets = std::make_unique<RenderTargets>(gpuContext);
    m_renderTargets->configure(m_viewportWidth, m_viewportHeight, getWebGpuSurface().getSurfaceFormat());

    m_chunkRenderManager = std::make_unique<ChunkRenderManager>(device, getWorld());

    createPipelines();

    m_profiler.init(gpuContext.getAdapter(), device);
}

void RendererSystem::createShadowResources() {
    auto& device = getWebGpuContext().getDevice();
    if (m_shadowDepthTexture) wgpuTextureRelease(m_shadowDepthTexture);
    if (m_shadowDepthView) wgpuTextureViewRelease(m_shadowDepthView);
    if (m_shadowSampler) wgpuSamplerRelease(m_shadowSampler);

    // Shadow depth texture
    WGPUTextureDescriptor shadowDepthDesc = {};
    shadowDepthDesc.dimension = WGPUTextureDimension_2D;
    shadowDepthDesc.size.width = m_shadowMapSize;
    shadowDepthDesc.size.height = m_shadowMapSize;
    shadowDepthDesc.size.depthOrArrayLayers = 1;
    shadowDepthDesc.mipLevelCount = 1;
    shadowDepthDesc.sampleCount = 1;
    shadowDepthDesc.format = WGPUTextureFormat_Depth32Float;
    shadowDepthDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    m_shadowDepthTexture = wgpuDeviceCreateTexture(device, &shadowDepthDesc);
    m_shadowDepthView = wgpuTextureCreateView(m_shadowDepthTexture, nullptr);

    // Shadow sampler
    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.compare = WGPUCompareFunction_Less;
    samplerDesc.maxAnisotropy = 1;
    m_shadowSampler = wgpuDeviceCreateSampler(device, &samplerDesc);

}

void RendererSystem::createShadowPipeline() {
    auto& device = getWebGpuContext().getDevice();
    auto artifacts = ShadowPipelineBuilder::build(device, m_shadowUniformsBuffer->get(), Chunk::WIDTH);
    m_shadowPipeline = artifacts.pipeline;
    m_shadowBindGroup = artifacts.bindGroup;
}

void RendererSystem::createFxaaPipeline() {
    auto& device = getWebGpuContext().getDevice();
    FxaaPipelineBuilder builder(device);
    m_fxaaPipeline = builder.build(m_fxaaBindGroupLayout, getWebGpuSurface().getSurfaceFormat());
}

void RendererSystem::createFxaaBindGroup() {
    auto& device = getWebGpuContext().getDevice();
    // Create sampler
    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.maxAnisotropy = 1;
    m_fxaaSampler = wgpuDeviceCreateSampler(device, &samplerDesc);

    // Create resolution uniform buffer
    float resolution[2] = {static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight)};
    WGPUBufferDescriptor resBufDesc = {};
    resBufDesc.size = sizeof(resolution);
    resBufDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    WGPUBuffer resBuffer = wgpuDeviceCreateBuffer(device, &resBufDesc);
    wgpuQueueWriteBuffer(m_queue, resBuffer, 0, resolution, sizeof(resolution));

    // Create bind group layout and store it
    WGPUBindGroupLayoutEntry entries[3] = {};
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Fragment;
    entries[0].texture.sampleType = WGPUTextureSampleType_Float;
    entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    entries[0].texture.multisampled = false;
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Fragment;
    entries[1].sampler.type = WGPUSamplerBindingType_Filtering;
    entries[2].binding = 2;
    entries[2].visibility = WGPUShaderStage_Fragment;
    entries[2].buffer.type = WGPUBufferBindingType_Uniform;
    entries[2].buffer.hasDynamicOffset = false;
    entries[2].buffer.minBindingSize = sizeof(resolution);
    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 3;
    bglDesc.entries = entries;
    if (m_fxaaBindGroupLayout) {
        wgpuBindGroupLayoutRelease(m_fxaaBindGroupLayout);
    }
    m_fxaaBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    // Create bind group
    WGPUBindGroupEntry bgEntries[3] = {};
    bgEntries[0].binding = 0;
    bgEntries[0].textureView = m_renderTargets->getSceneColorView();
    bgEntries[1].binding = 1;
    bgEntries[1].sampler = m_fxaaSampler;
    bgEntries[2].binding = 2;
    bgEntries[2].buffer = resBuffer;
    bgEntries[2].offset = 0;
    bgEntries[2].size = sizeof(resolution);
    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = m_fxaaBindGroupLayout;
    bgDesc.entryCount = 3;
    bgDesc.entries = bgEntries;
    m_fxaaBindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    wgpuBufferRelease(resBuffer);
}

void RendererSystem::render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) {
    updateUniformBuffer();

    // --- Shadow pass ---
    WGPURenderPassDepthStencilAttachment shadowDepthAttachment = {};
    shadowDepthAttachment.view = m_shadowDepthView;
    shadowDepthAttachment.depthLoadOp = WGPULoadOp_Clear;
    shadowDepthAttachment.depthStoreOp = WGPUStoreOp_Store;
    shadowDepthAttachment.depthClearValue = 1.0f;
    shadowDepthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
    shadowDepthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
    shadowDepthAttachment.stencilClearValue = 0;
    WGPURenderPassDescriptor shadowPassDesc = {};
    shadowPassDesc.colorAttachmentCount = 0;
    shadowPassDesc.colorAttachments = nullptr;
    shadowPassDesc.depthStencilAttachment = &shadowDepthAttachment;
    shadowPassDesc.label = WGPUStringView{"Shadow RenderPass", WGPU_STRLEN};
    WGPURenderPassEncoder shadowPass = wgpuCommandEncoderBeginRenderPass(encoder, &shadowPassDesc);
    wgpuRenderPassEncoderSetPipeline(shadowPass, m_shadowPipeline);
    wgpuRenderPassEncoderSetBindGroup(shadowPass, 0, m_shadowBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(shadowPass, 0, m_billboardVertexBuffer, 0, wgpuBufferGetSize(m_billboardVertexBuffer));
    wgpuRenderPassEncoderSetIndexBuffer(shadowPass, m_billboardIndexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(m_billboardIndexBuffer));
    const auto shadowChunkVertexBuffers = m_chunkRenderManager->getChunksToRender(
        getCamera().getPosition(),
        m_lightProjectionViewMatrix
    );
    for (auto &chunkVertexBuffer: shadowChunkVertexBuffers) {
        auto &[buffer, vertexCount] = chunkVertexBuffer;
        const uint64_t totalSize = wgpuBufferGetSize(buffer);
        const uint64_t chunkMetaOffset = totalSize - sizeof(glm::vec4);
        wgpuRenderPassEncoderSetVertexBuffer(shadowPass, 1, buffer, 0, chunkMetaOffset);
        wgpuRenderPassEncoderSetVertexBuffer(shadowPass, 2, buffer, chunkMetaOffset, sizeof(glm::vec4));
        wgpuRenderPassEncoderDrawIndexed(shadowPass, m_billboardIndexCount, vertexCount, 0, 0, 0);
    }
    wgpuRenderPassEncoderEnd(shadowPass);
    wgpuRenderPassEncoderRelease(shadowPass);

    // --- Main scene pass ---
    WGPURenderPassDescriptor scenePassDesc = {};
    scenePassDesc.nextInChain = nullptr;

    WGPURenderPassColorAttachment sceneColorAttachment = {};
    sceneColorAttachment.view = m_renderTargets->getSceneColorView();
    sceneColorAttachment.resolveTarget = nullptr;
    sceneColorAttachment.loadOp = WGPULoadOp_Clear;
    sceneColorAttachment.storeOp = WGPUStoreOp_Store;
    sceneColorAttachment.clearValue = WGPUColor{0.53, 0.81, 0.98, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
    sceneColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = m_renderTargets->getDepthView();
    depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.stencilClearValue = 0;

    scenePassDesc.colorAttachmentCount = 1;
    scenePassDesc.colorAttachments = &sceneColorAttachment;
    scenePassDesc.depthStencilAttachment = &depthStencilAttachment;
    scenePassDesc.label = WGPUStringView{"Scene RenderPass", WGPU_STRLEN};

    WGPURenderPassEncoder scenePass = wgpuCommandEncoderBeginRenderPass(encoder, &scenePassDesc);
    wgpuRenderPassEncoderSetPipeline(scenePass, m_renderPipeline);
    wgpuRenderPassEncoderSetBindGroup(scenePass, 0, m_uniformBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(scenePass, 0, m_billboardVertexBuffer, 0, wgpuBufferGetSize(m_billboardVertexBuffer));
    wgpuRenderPassEncoderSetIndexBuffer(scenePass, m_billboardIndexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(m_billboardIndexBuffer));

    auto& appData = getApplicationData();
    appData.renderedChunks = 0;
    appData.renderedVoxels = 0;

    const auto sceneChunkVertexBuffers = m_chunkRenderManager->getChunksToRender(getCamera());
    for (auto &chunkVertexBuffer: sceneChunkVertexBuffers) {
        auto &[buffer, vertexCount] = chunkVertexBuffer;

        appData.renderedChunks++;
        appData.renderedVoxels += vertexCount;

        const uint64_t totalSize = wgpuBufferGetSize(buffer);
        const uint64_t chunkMetaOffset = totalSize - sizeof(glm::vec4);

        wgpuRenderPassEncoderSetVertexBuffer(scenePass, 1, buffer, 0, chunkMetaOffset);
        wgpuRenderPassEncoderSetVertexBuffer(scenePass, 2, buffer, chunkMetaOffset, sizeof(glm::vec4));

        wgpuRenderPassEncoderDrawIndexed(scenePass, m_billboardIndexCount, vertexCount, 0, 0, 0);
    }
    wgpuRenderPassEncoderEnd(scenePass);
    wgpuRenderPassEncoderRelease(scenePass);

    WGPURenderPassColorAttachment fxaaColorAttachment = {};
    fxaaColorAttachment.view = targetView;
    fxaaColorAttachment.resolveTarget = nullptr;
    fxaaColorAttachment.loadOp = WGPULoadOp_Clear;
    fxaaColorAttachment.storeOp = WGPUStoreOp_Store;
    fxaaColorAttachment.clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
    fxaaColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU
    WGPURenderPassDescriptor fxaaPassDesc = {};
    fxaaPassDesc.nextInChain = nullptr;
    fxaaPassDesc.colorAttachmentCount = 1;
    fxaaPassDesc.colorAttachments = &fxaaColorAttachment;
    fxaaPassDesc.depthStencilAttachment = nullptr;
    fxaaPassDesc.label = WGPUStringView{"FXAA RenderPass", WGPU_STRLEN};

    WGPURenderPassEncoder fxaaPass = wgpuCommandEncoderBeginRenderPass(encoder, &fxaaPassDesc);
    wgpuRenderPassEncoderSetPipeline(fxaaPass, m_fxaaPipeline);
    wgpuRenderPassEncoderSetBindGroup(fxaaPass, 0, m_fxaaBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(fxaaPass, 0, m_billboardVertexBuffer, 0, wgpuBufferGetSize(m_billboardVertexBuffer));
    wgpuRenderPassEncoderSetIndexBuffer(fxaaPass, m_billboardIndexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(m_billboardIndexBuffer));
    wgpuRenderPassEncoderDrawIndexed(fxaaPass, m_billboardIndexCount, 1, 0, 0, 0);

    wgpuRenderPassEncoderEnd(fxaaPass);
    wgpuRenderPassEncoderRelease(fxaaPass);

    m_profiler.resolveAndCopy(encoder);
}

void RendererSystem::update(const float dt) {
    const Camera& camera = getCamera();
    m_chunkRenderManager->removeBuffersOfFarChunks(camera);

    m_timeAccumulator = std::fmod(m_timeAccumulator + dt, 1e+8f);
}

void RendererSystem::onEvent(Event &event) {
    EventDispatcher dispatcher(event);

    dispatcher.dispatch<WindowResizedEvent>([&](const WindowResizedEvent &windowResizedEvent) {
        LogApp::info("WindowResizedEvent: {}, {}", windowResizedEvent.getWidth(), windowResizedEvent.getHeight());

        m_viewportWidth = windowResizedEvent.getWidth();
        m_viewportHeight = windowResizedEvent.getHeight();

        m_renderTargets->configure(m_viewportWidth, m_viewportHeight, getWebGpuSurface().getSurfaceFormat());

        getCamera().setAspect(static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight));

        createPipelines();

        return true;
    });
}

void RendererSystem::setLighting(const bool lighting) {
    if (m_lighting != lighting) {
        m_lighting = lighting;
        createPipelines();
    }
}

void RendererSystem::setFog(const bool fog) {
    if (m_fog != fog) {
        m_fog = fog;
        createPipelines();
    }
}

void RendererSystem::setPointLight(const bool pointLight) {
    if (m_pointLight != pointLight) {
        m_pointLight = pointLight;
        createPipelines();
    }
}

void RendererSystem::exportTimestamps() const {
    const auto queue = wgpuDeviceGetQueue(getWebGpuContext().getDevice());
    const WGPUInstance& instance = getWebGpuContext().getInstance();
    m_profiler.exportTimestamps(instance, queue);
}

void RendererSystem::updateChunkVertexBuffer(const std::vector<VertexData>& vertexData, const glm::vec3& chunkPosition) const {
    m_chunkRenderManager->updateChunkVertexBuffer(vertexData, chunkPosition);
}

void RendererSystem::createPipelines() {
    createShadowResources();
    createShadowPipeline();

    createRenderPipeline();
    createFxaaBindGroup();
    createFxaaPipeline();
}

void RendererSystem::createRenderPipeline() {
    const auto device = getWebGpuContext().getDevice();

    const PipelineBuilder builder(device);
    const PipelineOptions opts{
        m_lighting,
        m_fog,
        m_pointLight,
        m_sampleCount,
        Chunk::WIDTH,
        getWebGpuSurface().getSurfaceFormat()
    };

    const auto& uniformBuffer = m_uniformsBuffer->get();
    const auto& blockTextureManager = getBlockTextureManager();
    const auto artifacts = builder.build(
        opts,
        uniformBuffer,
        blockTextureManager,
        m_shadowDepthView,
        m_shadowSampler
    );

    m_renderPipeline = artifacts.pipeline;
    m_uniformBindGroup = artifacts.uniformBindGroup;
}

void RendererSystem::initializeBuffers() {
    const auto device = getWebGpuContext().getDevice();

    // Vertex buffer data
    constexpr std::array vertexData{
        -1.0f, -1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f,
        -1.0f, 1.0f,
    };

    // Create vertex buffer
    WGPUBufferDescriptor bufferDesc{};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.size = vertexData.size() * sizeof(float);
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; // Vertex usage here!
    bufferDesc.mappedAtCreation = false;
    m_billboardVertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

    // Upload geometry data to the buffer
    wgpuQueueWriteBuffer(m_queue, m_billboardVertexBuffer, 0, vertexData.data(), bufferDesc.size);

    // Index buffer data
    constexpr std::array<uint16_t, 6> indexData{
        0, 1, 2,
        0, 2, 3,
    };
    m_billboardIndexCount = static_cast<uint32_t>(indexData.size());

    // Create index buffer
    WGPUBufferDescriptor indexBufferDesc{};
    indexBufferDesc.nextInChain = nullptr;
    indexBufferDesc.size = indexData.size() * sizeof(uint16_t);
    indexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
    indexBufferDesc.mappedAtCreation = false;
    m_billboardIndexBuffer = wgpuDeviceCreateBuffer(device, &indexBufferDesc);

    // Upload index data to the buffer
    wgpuQueueWriteBuffer(m_queue, m_billboardIndexBuffer, 0, indexData.data(), indexBufferDesc.size);
}

void RendererSystem::updateUniformBuffer() {
    if (!m_uniformsBuffer) return;
    if (!m_shadowUniformsBuffer) return;

    constexpr double orthoHalf = 96.0;
    constexpr double shadowNearPlane = -128.0;
    constexpr double shadowFarPlane = 128.0;

    const Camera& camera = getCamera();

    const glm::dvec3 lightDir = glm::normalize(glm::vec3(2.0, -7.0, 3.0));

    const glm::mat4 lightView = glm::lookAt(-lightDir, glm::dvec3(0), glm::dvec3(0, 1, 0));
    const glm::mat4 lightProj = glm::ortho(
                                    -orthoHalf, orthoHalf,
                                    -orthoHalf, orthoHalf,
                                    shadowNearPlane, shadowFarPlane);
    m_lightProjectionViewMatrix = lightProj * lightView;

    Uniforms uniforms{
        .projectionViewMatrix = camera.getProjectionViewMatrix(),
        .inverseProjectionViewMatrix = camera.getInverseProjectionViewMatrix(),
        .cameraPosition = camera.getPosition(),
        .fov = camera.getFov(),
        .viewportSize = glm::vec2(static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight)),
        .nearPlane = Camera::NEAR,
        .farPlane = Camera::FAR,
        .cameraDir = camera.getDirection(),
        .time = m_timeAccumulator,
        .lightProjectionViewMatrix = m_lightProjectionViewMatrix,
        .lightDirection = lightDir
    };
    m_uniformsBuffer->write(m_queue, uniforms);

    Uniforms shadowUniforms{
        .projectionViewMatrix = m_lightProjectionViewMatrix,
        .inverseProjectionViewMatrix = glm::inverse(m_lightProjectionViewMatrix),
        .cameraPosition = camera.getPosition(),
        .fov = 1.0f,
        .viewportSize = glm::vec2(static_cast<float>(m_shadowMapSize), static_cast<float>(m_shadowMapSize)),
        .nearPlane = shadowNearPlane,
        .farPlane = shadowFarPlane,
        .cameraDir = lightDir,
        .time = m_timeAccumulator,
        .lightProjectionViewMatrix = m_lightProjectionViewMatrix,
        .lightDirection = lightDir,
    };
    m_shadowUniformsBuffer->write(m_queue, shadowUniforms);
}
