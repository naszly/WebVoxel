#include "RendererSystem.h"

#include "application/Application.h"
#include "application/graphics/pipeline/PipelineBuilder.h"
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

    m_renderTargets = std::make_unique<RenderTargets>(gpuContext);
    m_renderTargets->configure(m_viewportWidth, m_viewportHeight, m_sampleCount, getWebGpuSurface().getSurfaceFormat());

    m_chunkRenderManager = std::make_unique<ChunkRenderManager>(device, getWorld());

    createRenderPipeline();

    m_profiler.init(gpuContext.getAdapter(), device);
}

void RendererSystem::render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) {
    const Camera& camera = getCamera();

    m_uniformData.projectionViewMatrix = camera.getProjectionViewMatrix();
    m_uniformData.inverseProjectionViewMatrix = camera.getInverseProjectionViewMatrix();
    m_uniformData.cameraPosition = camera.getPosition();
    m_uniformData.fov = camera.getFov();
    m_uniformData.viewportSize = {m_viewportWidth, m_viewportHeight};
    m_uniformData.nearPlane = Camera::NEAR;
    m_uniformData.farPlane = Camera::FAR;

    m_uniformsBuffer->write(m_queue, m_uniformData);

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;

    WGPURenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = m_renderTargets->colorAttachmentViewFor(targetView);
    renderPassColorAttachment.resolveTarget = m_renderTargets->resolveTargetFor(targetView);
    renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
    renderPassColorAttachment.clearValue = WGPUColor{0.02, 0.03, 0.06, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = m_renderTargets->getDepthView();
    depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.stencilClearValue = 0;

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

    m_profiler.attachTo(renderPassDesc);

    renderPassDesc.label = WGPUStringView{"RendererSystem RenderPass", WGPU_STRLEN};

    const WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    wgpuRenderPassEncoderSetPipeline(renderPass, m_renderPipeline);
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_uniformBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_billboardVertexBuffer, 0, wgpuBufferGetSize(m_billboardVertexBuffer));
    wgpuRenderPassEncoderSetIndexBuffer(renderPass, m_billboardIndexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(m_billboardIndexBuffer));

    auto& appData = getApplicationData();
    appData.renderedChunks = 0;
    appData.renderedVoxels = 0;

    auto chunkVertexBuffers = m_chunkRenderManager->getChunksToRender(camera);

    for (auto &chunkVertexBuffer: chunkVertexBuffers) {

        auto &[buffer, vertexCount] = chunkVertexBuffer;

        appData.renderedChunks++;
        appData.renderedVoxels += vertexCount;

        const uint64_t totalSize = wgpuBufferGetSize(buffer);
        const uint64_t chunkMetaOffset = totalSize - sizeof(glm::vec4);

        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 1, buffer, 0, chunkMetaOffset);
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 2, buffer, chunkMetaOffset, sizeof(glm::vec4));

        wgpuRenderPassEncoderDrawIndexed(renderPass, m_billboardIndexCount, vertexCount, 0, 0, 0);
    }

    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    m_profiler.resolveAndCopy(encoder);
}

void RendererSystem::update(const float dt) {
    const Camera& camera = getCamera();
    m_chunkRenderManager->removeBuffersOfFarChunks(camera);

    static float timeAccumulator = 0.0f;
    timeAccumulator += dt;
    if (timeAccumulator >= 1e+8) {
        timeAccumulator -= 1e+8;
    }
    m_uniformData.time = timeAccumulator;
}

void RendererSystem::onEvent(Event &event) {
    EventDispatcher dispatcher(event);

    dispatcher.dispatch<WindowResizedEvent>([&](const WindowResizedEvent &windowResizedEvent) {
        LogApp::info("WindowResizedEvent: {}, {}", windowResizedEvent.getWidth(), windowResizedEvent.getHeight());

        m_viewportWidth = windowResizedEvent.getWidth();
        m_viewportHeight = windowResizedEvent.getHeight();

        m_renderTargets->configure(m_viewportWidth, m_viewportHeight, m_sampleCount, getWebGpuSurface().getSurfaceFormat());

        getCamera().setAspect(static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight));

        return true;
    });
}

void RendererSystem::setLighting(const bool lighting) {
    if (m_lighting != lighting) {
        m_lighting = lighting;
        createRenderPipeline();
    }
}

void RendererSystem::setFog(const bool fog) {
    if (m_fog != fog) {
        m_fog = fog;
        createRenderPipeline();
    }
}

void RendererSystem::setPointLight(const bool pointLight) {
    if (m_pointLight != pointLight) {
        m_pointLight = pointLight;
        createRenderPipeline();
    }
}

void RendererSystem::setMultisampling(const bool multisampling) {
    if (multisampling != (m_sampleCount > 1)) {
        m_sampleCount = multisampling ? 4 : 1;
        m_renderTargets->configure(m_viewportWidth, m_viewportHeight, m_sampleCount, getWebGpuSurface().getSurfaceFormat());
        createRenderPipeline();
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
    const auto [pipeline, uniformBindGroup] = builder.build(opts, uniformBuffer, blockTextureManager);

    m_renderPipeline = pipeline;
    m_uniformBindGroup = uniformBindGroup;
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
