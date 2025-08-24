#include "RendererSystem.h"

#include "application/Application.h"
#include "application/domain/BlockLightInfo.h"
#include "core/events/ApplicationEvent.h"
#include "common/Log.h"
#include "common/FileSystem.h"

void RendererSystem::initialize() {
    LogApp::info("RendererSystem::initialize");

    m_queue = wgpuDeviceGetQueue(getWebGpuContext().getDevice());

    m_viewportWidth = getWebGpuSurface().getWidth();
    m_viewportHeight = getWebGpuSurface().getHeight();

    getCamera().setAspect(static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight));

    initializeBuffers();
    createDepthTexture();
    createRenderPipeline();

    // Initialize GPUQuerySet for benchmarking
    if (wgpuAdapterHasFeature(getWebGpuContext().getAdapter(), WGPUFeatureName_TimestampQuery)) {
        const auto device = getWebGpuContext().getDevice();

        WGPUQuerySetDescriptor querySetDesc = {};
        querySetDesc.count = 2;
        querySetDesc.type = WGPUQueryType_Timestamp;
        m_querySet = wgpuDeviceCreateQuerySet(device, &querySetDesc);

        // 1. Query resolve buffer (GPU-only)
        WGPUBufferDescriptor resolveBufferDesc = {};
        resolveBufferDesc.size = 2 * sizeof(uint64_t);
        resolveBufferDesc.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
        resolveBufferDesc.mappedAtCreation = false;
        resolveBufferDesc.label = WGPUStringView{"Query Resolve Buffer", WGPU_STRLEN};
        m_queryResolveBuffer = wgpuDeviceCreateBuffer(device, &resolveBufferDesc);

        // 2. Read buffer (CPU-visible)
        WGPUBufferDescriptor readBufferDesc = {};
        readBufferDesc.size = m_queryReadBufferCapacity;
        readBufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        readBufferDesc.mappedAtCreation = false;
        readBufferDesc.label = WGPUStringView{"Query Read Buffer", WGPU_STRLEN};
        m_queryReadBuffer = wgpuDeviceCreateBuffer(device, &readBufferDesc);
    } else {
        LogApp::warning("TimestampQuery feature not supported");
    }
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

    // Update uniform buffer data
    wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0, &m_uniformData, sizeof(Uniforms));

    // Create the render pass that clears the screen with our color
    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;

    // The attachment part of the render pass descriptor describes the target texture of the pass
    WGPURenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = m_sampleCount > 1 ? m_multisampleColorTextureView : targetView;
    renderPassColorAttachment.resolveTarget = m_sampleCount > 1 ? targetView : nullptr;
    renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
    renderPassColorAttachment.clearValue = WGPUColor{0.66, 0.7, 0.9, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = m_depthTextureView;
    depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.stencilClearValue = 0;

    WGPUPassTimestampWrites timestampWrites = {};
    timestampWrites.querySet = m_querySet;
    timestampWrites.beginningOfPassWriteIndex = 0;
    timestampWrites.endOfPassWriteIndex = 1;

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
    renderPassDesc.timestampWrites = &timestampWrites;

    renderPassDesc.label = WGPUStringView{"RendererSystem RenderPass", WGPU_STRLEN};

    auto& appData = getApplicationData();

    const WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    // Select which render pipeline to use
    wgpuRenderPassEncoderSetPipeline(renderPass, m_renderPipeline);

    // Set the bind group
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_uniformBindGroup, 0, nullptr);

    // Set vertex buffer while encoding the render pass
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_billboardVertexBuffer, 0, wgpuBufferGetSize(m_billboardVertexBuffer));

    // Set index buffer
    wgpuRenderPassEncoderSetIndexBuffer(renderPass, m_billboardIndexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(m_billboardIndexBuffer));

    std::vector<std::pair<glm::vec3, ChunkVertexBuffer>> sortedChunks;

    for (auto &[position, chunkVertexBuffer] : m_chunkVertexBuffers) {

        if (chunkVertexBuffer.vertexCount == 0) {
            continue;
        }

        const auto chunkCenter = glm::vec3(position) * static_cast<float>(Chunk::WIDTH) + glm::vec3(Chunk::WIDTH / 2.0f);

        const float chunkSphereRadius = std::sqrt(3.0f) * static_cast<float>(Chunk::WIDTH) / 2.0f;

        if (!camera.isSphereInFrustum(chunkCenter, chunkSphereRadius)) {
            continue;
        }

        sortedChunks.emplace_back(chunkCenter, chunkVertexBuffer);
    }

    const glm::vec3 camPos = camera.getPosition();

    std::ranges::sort(sortedChunks, [&](const auto &a, const auto &b) {
        const float distA = glm::length2(a.first - camPos);
        const float distB = glm::length2(b.first - camPos);

        return distA < distB; // Front-to-back!
    });

    appData.renderedChunks = 0;
    appData.renderedVoxels = 0;

    for (auto &chunkVertexBuffer: sortedChunks | std::views::values) {

        auto &[buffer, vertexCount] = chunkVertexBuffer;

        appData.renderedChunks++;
        appData.renderedVoxels += vertexCount;

        // Set voxel buffer
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 1, buffer, 0, wgpuBufferGetSize(buffer) - sizeof(glm::vec4));

        // Set chunk buffer
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 2, buffer, wgpuBufferGetSize(buffer) - sizeof(glm::vec4), sizeof(glm::vec4));

        // Use instanced drawing
        wgpuRenderPassEncoderDrawIndexed(renderPass, m_billboardIndexCount, vertexCount, 0, 0, 0);
    }
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    if (wgpuBufferGetMapState(m_queryReadBuffer) == WGPUBufferMapState_Unmapped) {
        constexpr uint64_t bufferSize = 2 * sizeof(uint64_t);

        wgpuCommandEncoderResolveQuerySet(encoder, m_querySet, 0, 2, m_queryResolveBuffer, 0);

        if (m_queryReadBufferSize >= m_queryReadBufferCapacity) {
            m_queryReadBufferSize = 0;
        }

        m_queryReadBufferSize += bufferSize;

        wgpuCommandEncoderCopyBufferToBuffer(encoder,
                                             m_queryResolveBuffer,
                                             0,
                                             m_queryReadBuffer,
                                             m_queryReadBufferSize - bufferSize,
                                             bufferSize);
    } else {
        LogApp::warning("Query read buffer is mapped, skipping copy");
    }
}

void RendererSystem::update(float dt) {
    World &world = getWorld();
    const glm::vec3 playerPosition = getCamera().getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    auto dirtyChunks = collectDirtyChunks(world, playerChunk);
    processDirtyChunks(world, dirtyChunks);
    removeFarChunkBuffers(playerChunk);
}

std::vector<std::reference_wrapper<Chunk>> RendererSystem::collectDirtyChunks(World& world, const glm::ivec3& playerChunk) const {
    const auto& chunks = world.getChunks();
    std::vector<std::reference_wrapper<Chunk>> dirtyChunks;

    std::ranges::copy_if(chunks, std::back_inserter(dirtyChunks), [&](const Chunk &chunk) {
        return chunk.isGpuBufferDirty() && hasAllNeighbours(world, chunk);
    });

    std::ranges::sort(dirtyChunks, [&](const Chunk &a, const Chunk &b) {
        const auto aPos = a.getPosition();
        const auto bPos = b.getPosition();
        return Utils::distance(aPos, playerChunk) < Utils::distance(bPos, playerChunk);
    });

    return dirtyChunks;
}

void RendererSystem::processDirtyChunks(const World& world, const std::vector<std::reference_wrapper<Chunk>>& dirtyChunks) {
    const size_t maxChunksToProcess = 6 + dirtyChunks.size() / 8;
    size_t processedChunks = 0;

    for (auto &chunkRef: dirtyChunks) {
        if (processedChunks >= maxChunksToProcess) {
            break;
        }

        auto& chunk = chunkRef.get();
        auto position = chunk.getPosition();
        auto chunkNeighborhood = world.getChunkNeighborhood(position);

        if (!chunkNeighborhood.hasAllNeighbours()) {
            continue;
        }

        ChunkVertexBuffer buffer;

        buffer = createChunkVertexBuffer(chunkNeighborhood);

        auto it = m_chunkVertexBuffers.find(position);
        if (it != m_chunkVertexBuffers.end()) {
            wgpuBufferRelease(it->second.buffer);
            if (buffer.vertexCount > 0) {
                it->second = buffer;
            } else {
                m_chunkVertexBuffers.erase(it);
            }
        } else if (buffer.vertexCount > 0) {
            m_chunkVertexBuffers.insert({position, buffer});
        }

        chunk.resetGpuBufferDirty();
        ++processedChunks;
    }
}

void RendererSystem::removeFarChunkBuffers(const glm::ivec3& playerChunk) {
    for (auto it = m_chunkVertexBuffers.begin(); it != m_chunkVertexBuffers.end();) {
        const auto &[chunkPosition, vertexBuffer] = *it;
        const int64_t distance = Utils::distance2(chunkPosition, playerChunk);
        constexpr auto farPlane = Camera::FAR / Chunk::WIDTH;
        if (distance > static_cast<int64_t>(farPlane * farPlane)) {
            wgpuBufferRelease(vertexBuffer.buffer);
            m_chunkVertexBuffers.erase(it++);
        } else {
            ++it;
        }
    }
}

void RendererSystem::onEvent(Event &event) {
    EventDispatcher dispatcher(event);

    dispatcher.dispatch<WindowResizedEvent>([&](const WindowResizedEvent &windowResizedEvent) {
        LogApp::info("WindowResizedEvent: {0}, {1}", windowResizedEvent.getWidth(), windowResizedEvent.getHeight());

        m_viewportWidth = windowResizedEvent.getWidth();
        m_viewportHeight = windowResizedEvent.getHeight();

        createDepthTexture();

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

void RendererSystem::exportTimestamps() const {
    const auto queue = wgpuDeviceGetQueue(getWebGpuContext().getDevice());

    WGPUQueueWorkDoneCallback onWorkDoneCallback = [](WGPUQueueWorkDoneStatus status, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
        const auto rendererSystem = static_cast<const RendererSystem*>(userdata1);
        if (status == WGPUQueueWorkDoneStatus_Success) {
            LogApp::info("Exporting timestamps...");
            rendererSystem->exportTimestampsInternal();
        } else {
            LogApp::error("Failed to export timestamps");
        }
    };

    WGPUQueueWorkDoneCallbackInfo workDoneInfo = {};
    workDoneInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    workDoneInfo.callback = onWorkDoneCallback;
    workDoneInfo.userdata1 = const_cast<RendererSystem *>(this);
    workDoneInfo.userdata2 = nullptr;
    workDoneInfo.nextInChain = nullptr;

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuQueueOnSubmittedWorkDone(queue, workDoneInfo);

    const WGPUInstance& instance = getWebGpuContext().getInstance();

    const auto waitStatus = wgpuInstanceWaitAny(instance, 1, &futureInfo, 6e+10);

    if (waitStatus != WGPUWaitStatus_Success) {
        LogApp::error("Failed to wait for work done: {0}", magic_enum::enum_name(waitStatus));
    }
}

void RendererSystem::createRenderPipeline() {
    const auto device = getWebGpuContext().getDevice();
    const auto surfaceFormat = getWebGpuSurface().getSurfaceFormat();

    // Load the shader module
    WGPUShaderModuleDescriptor shaderDesc{};
    std::string shaderCode = loadShader("shaders/shader.wgsl");

    WGPUShaderSourceWGSL shaderCodeDesc{};
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    shaderCodeDesc.code = WGPUStringView{shaderCode.c_str(), shaderCode.size()};
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    // Create the bind group layout for uniforms
    WGPUBindGroupLayoutEntry bglEntry{};
    bglEntry.binding = 0;
    bglEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bglEntry.buffer.type = WGPUBufferBindingType_Uniform;
    bglEntry.buffer.hasDynamicOffset = false;
    bglEntry.buffer.minBindingSize = sizeof(Uniforms);

    // Create the bind group layout for colors (storage buffer)
    WGPUBindGroupLayoutEntry colorBglEntry{};
    colorBglEntry.binding = 1;
    colorBglEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    colorBglEntry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    colorBglEntry.buffer.hasDynamicOffset = false;
    colorBglEntry.buffer.minBindingSize = 0;

    // Create the bind group layout for texture array
    WGPUBindGroupLayoutEntry textureArrayBglEntry{};
    textureArrayBglEntry.binding = 2;
    textureArrayBglEntry.visibility = WGPUShaderStage_Fragment;
    textureArrayBglEntry.texture.sampleType = WGPUTextureSampleType_Float;
    textureArrayBglEntry.texture.viewDimension = WGPUTextureViewDimension_2DArray;
    textureArrayBglEntry.texture.multisampled = false;

    std::array bglEntries{
        bglEntry,
        colorBglEntry,
        textureArrayBglEntry
    };
    WGPUBindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = bglEntries.size();
    bglDesc.entries = bglEntries.data();
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    // Create the bind group for uniforms
    WGPUBindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer = m_uniformBuffer;
    bgEntry.offset = 0;
    bgEntry.size = sizeof(Uniforms);

    auto& blockTextureManager = getBlockTextureManager();

    // Create the bind group for texture ids
    WGPUBindGroupEntry textureIdsBgEntry{};
    textureIdsBgEntry.binding = 1;
    textureIdsBgEntry.buffer = blockTextureManager.getTextureIdsBuffer();
    textureIdsBgEntry.offset = 0;
    textureIdsBgEntry.size = blockTextureManager.getTextureIdsBufferSize();

    // Create the bind group for texture array
    WGPUBindGroupEntry textureArrayBgEntry{};
    textureArrayBgEntry.binding = 2;
    textureArrayBgEntry.sampler = nullptr;
    textureArrayBgEntry.textureView = blockTextureManager.getTextureArrayView();

    std::array bgEntries{
        bgEntry,
        textureIdsBgEntry,
        textureArrayBgEntry
    };
    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout = bindGroupLayout;
    bgDesc.entryCount = bgEntries.size();
    bgDesc.entries = bgEntries.data();
    m_uniformBindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    // Create the pipeline layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    // Create the render pipeline
    WGPURenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.nextInChain = nullptr;
    pipelineDesc.layout = pipelineLayout;

    // Configure the vertex pipeline
    WGPUVertexBufferLayout billboardVertexBufferLayout{};
    WGPUVertexAttribute billboardAttributes{};
    billboardAttributes.shaderLocation = 0;
    billboardAttributes.format = WGPUVertexFormat_Float32x2;
    billboardAttributes.offset = 0;

    billboardVertexBufferLayout.attributeCount = 1;
    billboardVertexBufferLayout.attributes = &billboardAttributes;
    billboardVertexBufferLayout.arrayStride = 2 * sizeof(float);
    billboardVertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    // Configure the instance buffer layout
    WGPUVertexBufferLayout voxelVertexBufferLayout{};
    constexpr std::array voxelAttributes{
        // Position
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Uint32,
            .offset = 0,
            .shaderLocation = 1,
        },
        // Id
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Uint32,
            .offset = offsetof(VertexData, voxel),
            .shaderLocation = 2,
        },
        // Ambient Occlusion
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Uint32,
            .offset = offsetof(VertexData, ambientOcclusion),
            .shaderLocation = 4,
        },
        // Light
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Uint32,
            .offset = offsetof(VertexData, light),
            .shaderLocation = 5,
        }
    };
    uint32_t voxelAttributeCount = voxelAttributes.size();

    voxelVertexBufferLayout.attributeCount = voxelAttributeCount;
    voxelVertexBufferLayout.attributes = voxelAttributes.data();
    voxelVertexBufferLayout.arrayStride = sizeof(VertexData);
    voxelVertexBufferLayout.stepMode = WGPUVertexStepMode_Instance;

    WGPUVertexBufferLayout chunkVertexBufferLayout{};
    constexpr std::array chunkAttributes{
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Float32x4,
            .offset = 0,
            .shaderLocation = 3,
        }
    };

    chunkVertexBufferLayout.attributeCount = chunkAttributes.size();
    chunkVertexBufferLayout.attributes = chunkAttributes.data();
    chunkVertexBufferLayout.arrayStride = 0;
    chunkVertexBufferLayout.stepMode = WGPUVertexStepMode_Instance;

    const std::array bufferLayouts{
        billboardVertexBufferLayout,
        voxelVertexBufferLayout,
        chunkVertexBufferLayout
    };

    constexpr std::array vertexConstants{
        WGPUConstantEntry{
            .key = WGPUStringView{"CHUNK_SIZE", WGPU_STRLEN},
            .value = Chunk::WIDTH,
        }
    };

    pipelineDesc.vertex.bufferCount = bufferLayouts.size();
    pipelineDesc.vertex.buffers = bufferLayouts.data();
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = WGPUStringView{"vsMain", WGPU_STRLEN};
    pipelineDesc.vertex.constantCount = vertexConstants.size();
    pipelineDesc.vertex.constants = vertexConstants.data();

    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;

    WGPUBlendState blendState{};
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;
    blendState.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget{};
    colorTarget.format = surfaceFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    const std::array fragmentConstants{
        WGPUConstantEntry{
            .key = WGPUStringView{"LIGHTING", WGPU_STRLEN},
            .value = static_cast<double>(m_lighting),
        },
        WGPUConstantEntry{
            .key = WGPUStringView{"FOG", WGPU_STRLEN},
            .value = static_cast<double>(m_fog),
        }
    };

    WGPUFragmentState fragmentState{};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = WGPUStringView{"fsMain", WGPU_STRLEN};
    fragmentState.constantCount = fragmentConstants.size();
    fragmentState.constants = fragmentConstants.data();

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    // Configure the depth-stencil state
    WGPUDepthStencilState depthStencilState{};
    depthStencilState.format = WGPUTextureFormat_Depth24PlusStencil8;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack = depthStencilState.stencilFront;

    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = &depthStencilState;
    pipelineDesc.multisample.count = m_sampleCount;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    m_renderPipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    wgpuShaderModuleRelease(shaderModule);
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuPipelineLayoutRelease(pipelineLayout);
}

void RendererSystem::createDepthTexture() {
    const auto device = getWebGpuContext().getDevice();

    if (m_depthTexture) {
        wgpuTextureRelease(m_depthTexture);
        wgpuTextureViewRelease(m_depthTextureView);
    }

    if (m_multisampleColorTexture) {
        wgpuTextureRelease(m_multisampleColorTexture);
        wgpuTextureViewRelease(m_multisampleColorTextureView);
    }

    // Create the depth texture
    WGPUTextureDescriptor depthTextureDesc = {};
    depthTextureDesc.size.width = m_viewportWidth;
    depthTextureDesc.size.height = m_viewportHeight;
    depthTextureDesc.size.depthOrArrayLayers = 1;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = m_sampleCount;
    depthTextureDesc.dimension = WGPUTextureDimension_2D;
    depthTextureDesc.format = WGPUTextureFormat_Depth24PlusStencil8;
    depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;

    m_depthTexture = wgpuDeviceCreateTexture(device, &depthTextureDesc);
    if (!m_depthTexture) {
        LogApp::error("Failed to create depth texture");
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
        LogApp::error("Failed to create depth texture view");
        wgpuTextureRelease(m_depthTexture);
    }

    // Create the color texture
    WGPUTextureDescriptor colorTextureDesc = {};
    colorTextureDesc.size.width = m_viewportWidth;
    colorTextureDesc.size.height = m_viewportHeight;
    colorTextureDesc.size.depthOrArrayLayers = 1;
    colorTextureDesc.mipLevelCount = 1;
    colorTextureDesc.sampleCount = m_sampleCount;
    colorTextureDesc.dimension = WGPUTextureDimension_2D;
    colorTextureDesc.format = getWebGpuSurface().getSurfaceFormat();
    colorTextureDesc.usage = WGPUTextureUsage_RenderAttachment;

    m_multisampleColorTexture = wgpuDeviceCreateTexture(device, &colorTextureDesc);
    if (!m_multisampleColorTexture) {
        LogApp::error("Failed to create color texture");
        return;
    }

    WGPUTextureViewDescriptor colorViewDesc = {};
    colorViewDesc.format = getWebGpuSurface().getSurfaceFormat();
    colorViewDesc.dimension = WGPUTextureViewDimension_2D;
    colorViewDesc.baseMipLevel = 0;
    colorViewDesc.mipLevelCount = 1;
    colorViewDesc.baseArrayLayer = 0;
    colorViewDesc.arrayLayerCount = 1;
    colorViewDesc.aspect = WGPUTextureAspect_All;

    m_multisampleColorTextureView = wgpuTextureCreateView(m_multisampleColorTexture, &colorViewDesc);
    if (!m_multisampleColorTextureView) {
        LogApp::error("Failed to create color texture view");
        wgpuTextureRelease(m_multisampleColorTexture);
    }
}

std::string RendererSystem::loadShader(const char *filename) {
    auto shaderBuffer = FileSystem::readFileNative(filename);

    std::string shaderCode(shaderBuffer.begin(), shaderBuffer.end());

    return shaderCode;
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

    // Create uniform buffer
    WGPUBufferDescriptor uniformBufferDesc{};
    uniformBufferDesc.nextInChain = nullptr;
    uniformBufferDesc.size = sizeof(Uniforms);
    uniformBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    uniformBufferDesc.mappedAtCreation = false;
    m_uniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);
}

void RendererSystem::exportTimestampsInternal() const {
    struct UserData {
        WGPUBuffer buffer;
        uint64_t size;
        std::vector<uint64_t> durations;
    };

    auto callback = [](const WGPUMapAsyncStatus status,
                       WGPUStringView message,
                       WGPU_NULLABLE void* userdata1,
                       WGPU_NULLABLE void* userdata2)
    {
        const auto data = static_cast<UserData*>(userdata1);
        if (status == WGPUMapAsyncStatus_Success) {
            if (const auto* timestamps = static_cast<const uint64_t*>(wgpuBufferGetConstMappedRange(data->buffer, 0, data->size))) {
                for (size_t i = 0; i < data->size / sizeof(uint64_t) / 2; ++i) {
                    uint64_t duration = timestamps[i*2+1] - timestamps[i*2];
                    data->durations.push_back(duration);
                }
            } else {
                LogApp::error("Failed to get mapped range from query read buffer");
            }
            wgpuBufferUnmap(data->buffer);
        } else {
            LogApp::error("Failed to map query read buffer: {0}", magic_enum::enum_name(status));
        }
    };

    UserData userData{m_queryReadBuffer, m_queryReadBufferSize};

    auto callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = callback;
    callbackInfo.userdata1 = &userData;

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuBufferMapAsync(m_queryReadBuffer, WGPUMapMode_Read, 0, m_queryReadBufferCapacity, callbackInfo);

    const WGPUInstance& instance = getWebGpuContext().getInstance();

    const auto waitStatus = wgpuInstanceWaitAny(instance, 1, &futureInfo, 6e+10);

    if (waitStatus != WGPUWaitStatus_Success) {
        LogApp::error("Failed to wait for buffer map async: {0}", magic_enum::enum_name(waitStatus));
        return;
    }

    if (!userData.durations.empty()) {
        std::stringstream ss;
        for (const auto& duration : userData.durations) {
            ss << duration << "\n";
        }
        const std::string fileName = std::format("timestamps_{}.txt", std::chrono::high_resolution_clock::now().time_since_epoch().count());
        FileSystem::writeFile(fileName, ss.str().c_str(), ss.str().size());
        LogApp::info("Timestamps saved to file: {0}", fileName);

        FileSystem::download(fileName, fileName);
    } else {
        LogApp::warning("No timestamps to save");
    }
}

bool RendererSystem::hasAllNeighbours(const World& world, const Chunk& chunk) {
    const auto neighbours = world.getChunkNeighborhood(chunk.getPosition());
    return neighbours.hasAllNeighbours();
}

bool RendererSystem::testBitmaps(const ChunkNeighbourhoodBitmaps& bitmaps, const uint32_t x, const uint32_t y, const uint32_t z) {
    assert(x < 3 * Chunk::WIDTH && y < 3 * Chunk::WIDTH && z < 3 * Chunk::WIDTH);
    const uint32_t cnx = x / Chunk::WIDTH;
    const uint32_t cny = y / Chunk::WIDTH;
    const uint32_t cnz = z / Chunk::WIDTH;
    const uint32_t bx = x % Chunk::WIDTH;
    const uint32_t by = y % Chunk::WIDTH;
    const uint32_t bz = z % Chunk::WIDTH;
    const uint32_t index = bx * Chunk::WIDTH * Chunk::WIDTH + by * Chunk::WIDTH + bz;
    return bitmaps[cnx][cny][cnz]->test(index);
}

RendererSystem::ChunkVertexBuffer RendererSystem::createChunkVertexBuffer(const ChunkNeighborhood& neighborChunks) const {
    ChunkVertexBuffer vertexBuffer;

    static std::vector<VertexData> points;
    points.clear();

    getVertices(neighborChunks, points);

    if (points.empty()) {
        return vertexBuffer;
    }

    const auto device = getWebGpuContext().getDevice();
    const auto queue = wgpuDeviceGetQueue(device);

    const auto& centerChunk = *neighborChunks.getChunk(0,0,0);
    const auto chunkPosition = glm::vec4(centerChunk.getPosition(), 0.0f);

    const size_t bufferSize = points.size() * sizeof(VertexData) + sizeof(chunkPosition);

    WGPUBufferDescriptor descriptor{};
    descriptor.size = bufferSize;
    descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    descriptor.mappedAtCreation = false;
    descriptor.label = WGPUStringView{"Chunk Vertex Buffer", WGPU_STRLEN};

    vertexBuffer.buffer = wgpuDeviceCreateBuffer(device, &descriptor);
    vertexBuffer.vertexCount = points.size();

    static std::vector<uint8_t> data;
    if (data.size() < bufferSize) {
        data.resize(bufferSize);
    }

    memcpy(data.data(), points.data(), points.size() * sizeof(VertexData));
    memcpy(data.data() + points.size() * sizeof(VertexData), &chunkPosition, sizeof(chunkPosition));

    wgpuQueueWriteBuffer(queue, vertexBuffer.buffer, 0, data.data(), bufferSize);

    return vertexBuffer;
}

void RendererSystem::getVertices(const ChunkNeighborhood& neighborChunks, std::vector<VertexData> &vertices) {
    const auto& centerChunk = *neighborChunks.getChunk(0,0,0);

    // Find all light sources in the chunk
    std::vector<Chunk::LightSource> lights;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                const auto& chunk = *neighborChunks.getChunk(dx, dy, dz);
                auto chunkLight = chunk.getLightSources(dx, dy, dz);
                lights.insert(lights.end(), chunkLight.begin(), chunkLight.end());
            }
        }
    }

    const ChunkNeighbourhoodBitmaps neighbourhoodBitmaps{
        std::array{
            std::array{&neighborChunks.getChunk(-1,-1,-1)->getBitmap(), &neighborChunks.getChunk(-1,-1,0)->getBitmap(), &neighborChunks.getChunk(-1,-1,1)->getBitmap()},
            std::array{&neighborChunks.getChunk(-1,0,-1)->getBitmap(),  &neighborChunks.getChunk(-1,0,0)->getBitmap(),  &neighborChunks.getChunk(-1,0,1)->getBitmap()},
            std::array{&neighborChunks.getChunk(-1,1,-1)->getBitmap(),  &neighborChunks.getChunk(-1,1,0)->getBitmap(),  &neighborChunks.getChunk(-1,1,1)->getBitmap()}
        },
        std::array{
            std::array{&neighborChunks.getChunk(0,-1,-1)->getBitmap(),  &neighborChunks.getChunk(0,-1,0)->getBitmap(),  &neighborChunks.getChunk(0,-1,1)->getBitmap()},
            std::array{&neighborChunks.getChunk(0,0,-1)->getBitmap(),   &neighborChunks.getChunk(0,0,0)->getBitmap(),   &neighborChunks.getChunk(0,0,1)->getBitmap()},
            std::array{&neighborChunks.getChunk(0,1,-1)->getBitmap(),   &neighborChunks.getChunk(0,1,0)->getBitmap(),   &neighborChunks.getChunk(0,1,1)->getBitmap()}
        },
        std::array{
            std::array{&neighborChunks.getChunk(1,-1,-1)->getBitmap(),  &neighborChunks.getChunk(1,-1,0)->getBitmap(),  &neighborChunks.getChunk(1,-1,1)->getBitmap()},
            std::array{&neighborChunks.getChunk(1,0,-1)->getBitmap(),   &neighborChunks.getChunk(1,0,0)->getBitmap(),   &neighborChunks.getChunk(1,0,1)->getBitmap()},
            std::array{&neighborChunks.getChunk(1,1,-1)->getBitmap(),   &neighborChunks.getChunk(1,1,0)->getBitmap(),   &neighborChunks.getChunk(1,1,1)->getBitmap()}
        }
    };

    const auto& lightMap = propagateLight(neighbourhoodBitmaps, lights);

    // The chunk is now in the center of a 3*WIDTH bitmap, so valid voxel region is:
    // x, y, z in [WIDTH, 2*WIDTH)
    for (uint32_t x = Chunk::WIDTH; x < 2 * Chunk::WIDTH; ++x) {
        for (uint32_t y = Chunk::WIDTH; y < 2 * Chunk::WIDTH; ++y) {
            for (uint32_t z = Chunk::WIDTH; z < 2 * Chunk::WIDTH; ++z) {
                if (testBitmaps(neighbourhoodBitmaps, x, y, z)) {
                    // Check if not fully surrounded
                    const bool nx = testBitmaps(neighbourhoodBitmaps, x-1, y, z);
                    const bool px = testBitmaps(neighbourhoodBitmaps, x+1, y, z);
                    const bool ny = testBitmaps(neighbourhoodBitmaps, x, y-1, z);
                    const bool py = testBitmaps(neighbourhoodBitmaps, x, y+1, z);
                    const bool nz = testBitmaps(neighbourhoodBitmaps, x, y, z-1);
                    const bool pz = testBitmaps(neighbourhoodBitmaps, x, y, z+1);
                    const bool surrounded = nx & px & ny & py & nz & pz;
                    if (!surrounded) {
                        const uint8_t vx = x - Chunk::WIDTH;
                        const uint8_t vy = y - Chunk::WIDTH;
                        const uint8_t vz = z - Chunk::WIDTH;
                        if (vx < Chunk::WIDTH && vy < Chunk::WIDTH && vz < Chunk::WIDTH) {
                            const auto& voxel = centerChunk.getVoxel(vx, vy, vz);

                            const auto& faceNxLightIntensity = lightMap[x - 1][y][z];
                            const auto& facePxLightIntensity = lightMap[x + 1][y][z];
                            const auto& faceNyLightIntensity = lightMap[x][y - 1][z];
                            const auto& facePyLightIntensity = lightMap[x][y + 1][z];
                            const auto& faceNzLightIntensity = lightMap[x][y][z - 1];
                            const auto& facePzLightIntensity = lightMap[x][y][z + 1];
                            const auto voxelLight = PackedLight(faceNxLightIntensity, facePxLightIntensity,
                                                                faceNyLightIntensity, facePyLightIntensity,
                                                                faceNzLightIntensity, facePzLightIntensity);

                            VertexData voxelData = {
                                vx, vy, vz, 1, voxel,
                                getAmbientOcclusion(neighbourhoodBitmaps, x, y, z),
                                voxelLight
                            };

                            vertices.emplace_back(voxelData);
                        }
                    }
                }
            }
        }
    }
}

AmbientOcclusion RendererSystem::getAmbientOcclusion(const ChunkNeighbourhoodBitmaps &bitmaps,
                                                     const uint32_t x, const uint32_t y, const uint32_t z) {
    struct OffsetFlag {
        int dx, dy, dz;
        AmbientOcclusion flag;
    };

    static constexpr OffsetFlag table[] = {
        {-1, -1, -1, AmbientOcclusion::CornerNxNyNz},
        {-1, -1,  1, AmbientOcclusion::CornerNxNyPz},
        {-1,  1, -1, AmbientOcclusion::CornerNxPyNz},
        {-1,  1,  1, AmbientOcclusion::CornerNxPyPz},
        { 1, -1, -1, AmbientOcclusion::CornerPxNyNz},
        { 1, -1,  1, AmbientOcclusion::CornerPxNyPz},
        { 1,  1, -1, AmbientOcclusion::CornerPxPyNz},
        { 1,  1,  1, AmbientOcclusion::CornerPxPyPz},
        {-1, -1,  0, AmbientOcclusion::EdgeNxNy},
        {-1,  1,  0, AmbientOcclusion::EdgeNxPy},
        { 1, -1,  0, AmbientOcclusion::EdgePxNy},
        { 1,  1,  0, AmbientOcclusion::EdgePxPy},
        {-1,  0, -1, AmbientOcclusion::EdgeNxNz},
        {-1,  0,  1, AmbientOcclusion::EdgeNxPz},
        { 1,  0, -1, AmbientOcclusion::EdgePxNz},
        { 1,  0,  1, AmbientOcclusion::EdgePxPz},
        { 0, -1, -1, AmbientOcclusion::EdgeNyNz},
        { 0, -1,  1, AmbientOcclusion::EdgeNyPz},
        { 0,  1, -1, AmbientOcclusion::EdgePyNz},
        { 0,  1,  1, AmbientOcclusion::EdgePyPz},
    };

    auto ao = AmbientOcclusion::None;
    for (const auto& [dx, dy, dz, flag] : table) {
        const uint32_t mask = testBitmaps(bitmaps, dx + x, dy + y, dz + z);
        ao |= static_cast<AmbientOcclusion>(mask * static_cast<uint32_t>(flag));
    }
    return ao;
}

RendererSystem::LightMap& RendererSystem::propagateLight(const ChunkNeighbourhoodBitmaps& bitmaps,
                                                         const std::vector<Chunk::LightSource>& lights) {
    static constexpr int LIGHT_GRID_DIM = Chunk::WIDTH * 3;
    static constexpr int LIGHT_GRID_SIZE = LIGHT_GRID_DIM * LIGHT_GRID_DIM * LIGHT_GRID_DIM;
    static constexpr int NEIGHBOR_DIRS = 6;

    struct Storage {
        LightMap map{};
        CircularBuffer<Chunk::LightSource, LIGHT_GRID_SIZE> queue{};
    };
    static auto storage = std::make_unique<Storage>();
    auto& lightMap = storage->map;
    auto& queue    = storage->queue;

    static_assert(std::is_trivially_copyable_v<BlockLightInfo>);
    std::fill_n(&lightMap[0][0][0], LIGHT_GRID_SIZE, BlockLightInfo{});

    queue.clear();

    for (const auto& [lx0, ly0, lz0, lightInfo] : lights) {
        const int gx = lx0 + Chunk::WIDTH;
        const int gy = ly0 + Chunk::WIDTH;
        const int gz = lz0 + Chunk::WIDTH;
        if (gx < 0 || gy < 0 || gz < 0 ||
            gx >= LIGHT_GRID_DIM || gy >= LIGHT_GRID_DIM || gz >= LIGHT_GRID_DIM)
            continue;
        queue.push({gx, gy, gz, lightInfo});
        lightMap[gx][gy][gz] = lightInfo;
    }

    static constexpr int DX[NEIGHBOR_DIRS] = { 1,-1, 0, 0, 0, 0 };
    static constexpr int DY[NEIGHBOR_DIRS] = { 0, 0, 1,-1, 0, 0 };
    static constexpr int DZ[NEIGHBOR_DIRS] = { 0, 0, 0, 0, 1,-1 };

    while (!queue.empty()) {
        auto [x, y, z, srcLight] = queue.pop();
        const int next = srcLight.getIntensity() - 1;
        if (next < 0) continue;

        for (int d = 0; d < NEIGHBOR_DIRS; ++d) {
            const int nx = x + DX[d];
            const int ny = y + DY[d];
            const int nz = z + DZ[d];

            if (nx < 0 || ny < 0 || nz < 0 ||
                nx >= LIGHT_GRID_DIM || ny >= LIGHT_GRID_DIM || nz >= LIGHT_GRID_DIM)
                continue;

            if (testBitmaps(bitmaps, nx, ny, nz))
                continue;

            auto& dst = lightMap[nx][ny][nz];
            if (dst.getIntensity() >= next)
                continue;

            dst.setIntensity(next);
            queue.push({nx, ny, nz, dst});
        }
    }

    return lightMap;
}