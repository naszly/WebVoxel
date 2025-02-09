#include "RendererSystem.h"

#include <fstream>

#include "../Application.h"
#include "../ApplicationEvent.h"
#include "../Log.h"

void RendererSystem::initialize() {
    LogApp::info("RendererSystem::initialize");

    m_Queue = wgpuDeviceGetQueue(GetWebGPUContext().getDevice());

    m_ViewportWidth = GetWebGPUSurface().getWidth();
    m_ViewportHeight = GetWebGPUSurface().getHeight();

    Camera &camera = Application::GetInstance().getCamera();

    camera.setPerspective(FOV, static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight));

    InitializeBuffers();
    createDepthTexture();
    createRenderPipeline();
}

void RendererSystem::render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) {
    const Camera &camera = Application::GetInstance().getCamera();

    m_UniformData.projectionViewMatrix = camera.getProjectionViewMatrix();
    m_UniformData.inverseProjectionViewMatrix = camera.getInverseProjectionViewMatrix();
    m_UniformData.cameraPosition = camera.getPosition();
    m_UniformData.fov = FOV;
    m_UniformData.viewportSize = {m_ViewportWidth, m_ViewportHeight};
    m_UniformData.nearPlane = Camera::NEAR;
    m_UniformData.farPlane = Camera::FAR;

    // Update uniform buffer data
    wgpuQueueWriteBuffer(m_Queue, m_UniformBuffer, 0, &m_UniformData, sizeof(Uniforms));

    // Create the render pass that clears the screen with our color
    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;

    // The attachment part of the render pass descriptor describes the target texture of the pass
    WGPURenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = m_MultisampleColorTextureView;
    renderPassColorAttachment.resolveTarget = targetView; // Resolve to the target view
    renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
    renderPassColorAttachment.clearValue = WGPUColor{0.66, 0.7, 0.9, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = m_DepthTextureView;
    depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.stencilClearValue = 0;

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
    renderPassDesc.timestampWrites = nullptr;
    renderPassDesc.label = "RendererSystem RenderPass";

    Application &app = Application::GetInstance();
    auto &appData = app.getApplicationData();

    const WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    // Select which render pipeline to use
    wgpuRenderPassEncoderSetPipeline(renderPass, m_RenderPipeline);

    // Set the bind group
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_UniformBindGroup, 0, nullptr);

    // Set vertex buffer while encoding the render pass
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_BillboardVertexBuffer, 0, wgpuBufferGetSize(m_BillboardVertexBuffer));

    // Set index buffer
    wgpuRenderPassEncoderSetIndexBuffer(renderPass, m_BillboardIndexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(m_BillboardIndexBuffer));

    appData.renderedChunks = 0;
    appData.renderedVoxels = 0;
    for (auto &[position, chunkVertexBuffer]: m_ChunkVertexBuffers) {

        auto &[buffer, vertexCount] = chunkVertexBuffer;

        if (vertexCount == 0) {
            continue;
        }

        const auto chunkCenter = glm::vec3(position) * static_cast<float>(Chunk::SIZE) + glm::vec3(Chunk::SIZE / 2.0f);

        const auto chunkSphereRadius = glm::sqrt(3.0f) * static_cast<float>(Chunk::SIZE) / 2.0f;

        if (!camera.isSphereInFrustum(chunkCenter, chunkSphereRadius)) {
            continue;
        }

        appData.renderedChunks++;
        appData.renderedVoxels += vertexCount;

        // Update uniform buffer data
        wgpuQueueWriteBuffer(m_Queue, m_UniformBuffer, 0, &m_UniformData, sizeof(Uniforms));

        // Set voxel buffer
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 1, buffer, 0, wgpuBufferGetSize(buffer) - sizeof(glm::vec4));

        // Set chunk buffer
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 2, buffer, wgpuBufferGetSize(buffer) - sizeof(glm::vec4), sizeof(glm::vec4));

        // Use instanced drawing
        wgpuRenderPassEncoderDrawIndexed(renderPass, m_BillboardIndexCount, vertexCount, 0, 0, 0);
    }
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);
}

void RendererSystem::update(float dt) {
    World &world = GetWorld();

    const auto chunks = world.getChunks();

    for (auto &chunk: chunks) {
        if (chunk.isDirty()) {
            auto position = chunk.getPosition();
            auto neighbours = world.getChunkNeighbours(position);

            if (!neighbours.hasAllNeighbours()) {
                continue;
            }

            auto bitmap = chunk.getBitmap(neighbours);

            auto buffer = createChunkVertexBuffer(position, bitmap, [&](const uint32_t x, const uint32_t y, const uint32_t z) {
                return chunk.getVoxel(x, y, z);
            });

            auto it = m_ChunkVertexBuffers.find(position);
            if (it != m_ChunkVertexBuffers.end()) {
                wgpuBufferRelease(it->second.buffer);
                it->second = buffer;
            } else {
                m_ChunkVertexBuffers.insert({position, buffer});
            }

            chunk.resetDirty();
        }
    }
}

void RendererSystem::onEvent(Event &event) {
    EventDispatcher dispatcher(event);

    dispatcher.dispatch<WindowResizedEvent>([&](const WindowResizedEvent &windowResizedEvent) {
        LogApp::info("WindowResizedEvent: {0}, {1}", windowResizedEvent.getWidth(), windowResizedEvent.getHeight());

        m_ViewportWidth = windowResizedEvent.getWidth();
        m_ViewportHeight = windowResizedEvent.getHeight();

        createDepthTexture();

        Camera &camera = Application::GetInstance().getCamera();
        camera.setPerspective(FOV, static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight));

        return true;
    });
}

void RendererSystem::createRenderPipeline() {
    const auto device = GetWebGPUContext().getDevice();
    const auto surfaceFormat = GetWebGPUSurface().getSurfaceFormat();

    // Load the shader module
    WGPUShaderModuleDescriptor shaderDesc{};
    std::string shaderCode = LoadShader("shaders/shader.wgsl");

    WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};
    shaderCodeDesc.chain.next = nullptr;
#ifdef WEBGPU_BACKEND_DAWN
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
#else
	shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
#endif
    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    shaderCodeDesc.code = shaderCode.c_str();
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    // Create the bind group layout
    WGPUBindGroupLayoutEntry bglEntry{};
    bglEntry.binding = 0;
    bglEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bglEntry.buffer.type = WGPUBufferBindingType_Uniform;
    bglEntry.buffer.hasDynamicOffset = false;
    bglEntry.buffer.minBindingSize = sizeof(Uniforms);

    WGPUBindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    // Create the bind group
    WGPUBindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer = m_UniformBuffer;
    bgEntry.offset = 0;
    bgEntry.size = sizeof(Uniforms);

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout = bindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    m_UniformBindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

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
        // Color
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Uint32,
            .offset = 1 * sizeof(uint32_t),
            .shaderLocation = 2,
        }
    };

    voxelVertexBufferLayout.attributeCount = voxelAttributes.size();
    voxelVertexBufferLayout.attributes = voxelAttributes.data();
    voxelVertexBufferLayout.arrayStride = 2 * sizeof(uint32_t);
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

    constexpr std::array pipelineConstants{
        WGPUConstantEntry{
            .key = "CHUNK_SIZE",
            .value = Chunk::SIZE,
        }
    };

    pipelineDesc.vertex.bufferCount = bufferLayouts.size();
    pipelineDesc.vertex.buffers = bufferLayouts.data();
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = pipelineConstants.size();
    pipelineDesc.vertex.constants = pipelineConstants.data();

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

    WGPUFragmentState fragmentState{};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    // Configure the depth-stencil state
    WGPUDepthStencilState depthStencilState{};
    depthStencilState.format = WGPUTextureFormat_Depth24PlusStencil8;
#ifdef WEBGPU_BACKEND_DAWN
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
#else
	depthStencilState.depthWriteEnabled = true;
#endif
    depthStencilState.depthCompare = WGPUCompareFunction_Less;
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack = depthStencilState.stencilFront;

    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = &depthStencilState;
    pipelineDesc.multisample.count = 4;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    m_RenderPipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    wgpuShaderModuleRelease(shaderModule);
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuPipelineLayoutRelease(pipelineLayout);
}

void RendererSystem::createDepthTexture() {
    const auto device = GetWebGPUContext().getDevice();

    if (m_DepthTexture) {
        wgpuTextureRelease(m_DepthTexture);
        wgpuTextureViewRelease(m_DepthTextureView);
    }

    if (m_MultisampleColorTexture) {
        wgpuTextureRelease(m_MultisampleColorTexture);
        wgpuTextureViewRelease(m_MultisampleColorTextureView);
    }

    // Create the depth texture
    WGPUTextureDescriptor depthTextureDesc = {};
    depthTextureDesc.size.width = m_ViewportWidth;
    depthTextureDesc.size.height = m_ViewportHeight;
    depthTextureDesc.size.depthOrArrayLayers = 1;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 4; // Ensure this matches the color texture sample count
    depthTextureDesc.dimension = WGPUTextureDimension_2D;
    depthTextureDesc.format = WGPUTextureFormat_Depth24PlusStencil8;
    depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;

    m_DepthTexture = wgpuDeviceCreateTexture(device, &depthTextureDesc);
    if (!m_DepthTexture) {
        LogCore::error("Failed to create depth texture");
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

    m_DepthTextureView = wgpuTextureCreateView(m_DepthTexture, &depthViewDesc);
    if (!m_DepthTextureView) {
        LogCore::error("Failed to create depth texture view");
        wgpuTextureRelease(m_DepthTexture);
    }

    // Create the color texture
    WGPUTextureDescriptor colorTextureDesc = {};
    colorTextureDesc.size.width = m_ViewportWidth;
    colorTextureDesc.size.height = m_ViewportHeight;
    colorTextureDesc.size.depthOrArrayLayers = 1;
    colorTextureDesc.mipLevelCount = 1;
    colorTextureDesc.sampleCount = 4; // Ensure this matches the depth texture sample count
    colorTextureDesc.dimension = WGPUTextureDimension_2D;
    colorTextureDesc.format = GetWebGPUSurface().getSurfaceFormat();
    colorTextureDesc.usage = WGPUTextureUsage_RenderAttachment;

    m_MultisampleColorTexture = wgpuDeviceCreateTexture(device, &colorTextureDesc);
    if (!m_MultisampleColorTexture) {
        LogCore::error("Failed to create color texture");
        return;
    }

    WGPUTextureViewDescriptor colorViewDesc = {};
    colorViewDesc.format = GetWebGPUSurface().getSurfaceFormat();
    colorViewDesc.dimension = WGPUTextureViewDimension_2D;
    colorViewDesc.baseMipLevel = 0;
    colorViewDesc.mipLevelCount = 1;
    colorViewDesc.baseArrayLayer = 0;
    colorViewDesc.arrayLayerCount = 1;
    colorViewDesc.aspect = WGPUTextureAspect_All;

    m_MultisampleColorTextureView = wgpuTextureCreateView(m_MultisampleColorTexture, &colorViewDesc);
    if (!m_MultisampleColorTextureView) {
        LogCore::error("Failed to create color texture view");
        wgpuTextureRelease(m_MultisampleColorTexture);
    }
}

std::string RendererSystem::LoadShader(const char *filename) {
    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        LogWebGPU::error("Failed to open file: {0}", filename);
        return "";
    }

    std::string shaderCode((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return shaderCode;
}

void RendererSystem::InitializeBuffers() {
    const auto device = GetWebGPUContext().getDevice();

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
    m_BillboardVertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

    // Upload geometry data to the buffer
    wgpuQueueWriteBuffer(m_Queue, m_BillboardVertexBuffer, 0, vertexData.data(), bufferDesc.size);

    // Index buffer data
    constexpr std::array<uint16_t, 6> indexData{
        0, 1, 2,
        0, 2, 3,
    };
    m_BillboardIndexCount = static_cast<uint32_t>(indexData.size());

    // Create index buffer
    WGPUBufferDescriptor indexBufferDesc{};
    indexBufferDesc.nextInChain = nullptr;
    indexBufferDesc.size = indexData.size() * sizeof(uint16_t);
    indexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
    indexBufferDesc.mappedAtCreation = false;
    m_BillboardIndexBuffer = wgpuDeviceCreateBuffer(device, &indexBufferDesc);

    // Upload index data to the buffer
    wgpuQueueWriteBuffer(m_Queue, m_BillboardIndexBuffer, 0, indexData.data(), indexBufferDesc.size);

    // Create uniform buffer
    WGPUBufferDescriptor uniformBufferDesc{};
    uniformBufferDesc.nextInChain = nullptr;
    uniformBufferDesc.size = sizeof(Uniforms);
    uniformBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    uniformBufferDesc.mappedAtCreation = false;
    m_UniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);
}

RendererSystem::ChunkVertexBuffer RendererSystem::createChunkVertexBuffer(const glm::ivec3 position,
    const ChunkBitmap &bitmap, const std::function<VoxelData(uint32_t, uint32_t, uint32_t)> &getVoxel) {
    ChunkVertexBuffer vertexBuffer;

    static std::vector<VertexData> points;
    points.clear();

    getVertices(bitmap, getVoxel, points);

    const auto device = GetWebGPUContext().getDevice();
    const auto queue = wgpuDeviceGetQueue(device);

    const auto chunkPosition = glm::vec4(position, 0.0f);

    const size_t bufferSize = points.size() * sizeof(VertexData) + sizeof(chunkPosition);

    WGPUBufferDescriptor descriptor{};
    descriptor.size = bufferSize;
    descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    descriptor.mappedAtCreation = false;
    descriptor.label = "Chunk Vertex Buffer";

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

void RendererSystem::getVertices(const ChunkBitmap &bitmap,
    const std::function<VoxelData(uint32_t, uint32_t, uint32_t)> &getVoxel, std::vector<VertexData> &vertices) {
    constexpr uint32_t BITMAP_SIZE_SQUARED = BITMAP_SIZE * BITMAP_SIZE;

    for (uint32_t i = BITMAP_SIZE_SQUARED; i < BITMAP_SIZE_SQUARED * (BITMAP_SIZE - 1); i++) {
        if (i % ChunkBitmap::WORD_SIZE == 0 && !bitmap.testWord(i / ChunkBitmap::WORD_SIZE)) {
            i += ChunkBitmap::WORD_SIZE - 1;
            continue;
        }

        const bool isVisible = bitmap.test(i) &
                               !(bitmap.test(i-1) & bitmap.test(i+1) &
                                 bitmap.test(i-BITMAP_SIZE) & bitmap.test(i+BITMAP_SIZE) &
                                 bitmap.test(i-BITMAP_SIZE_SQUARED) & bitmap.test(i+BITMAP_SIZE_SQUARED));

        if (isVisible) {
            const uint32_t x = (i / (BITMAP_SIZE_SQUARED)) - 1;
            const uint32_t y = ((i % (BITMAP_SIZE_SQUARED)) / BITMAP_SIZE) - 1;
            const uint32_t z = (i % BITMAP_SIZE) - 1;

            if (x < Chunk::SIZE && y < Chunk::SIZE && z < Chunk::SIZE) {
                const auto& voxel = getVoxel(x, y, z);
                vertices.emplace_back(x, y, z, 1, voxel);
            }
        }
    }
}
