#include "RendererSystem.h"

#include <fstream>

#include "../Application.h"
#include "../ApplicationEvent.h"
#include "../Log.h"

void RendererSystem::initialize() {
    LogApp::info("RendererSystem::initialize");

    m_Queue = wgpuDeviceGetQueue(GetWebGPUContext().getDevice());

    m_viewportWidth = GetWebGPUSurface().getWidth();
    m_viewportHeight = GetWebGPUSurface().getHeight();

    Camera &camera = Application::GetInstance().getCamera();
    constexpr float fov = glm::radians(66.0);
    camera.setPerspective(fov, (float) m_viewportWidth / (float) m_viewportHeight);

    InitializeBuffers();
    createDepthTexture();
    createRenderPipeline();
}

void RendererSystem::render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) {
    const Camera &camera = Application::GetInstance().getCamera();

    uniformData.projectionViewMatrix = camera.getProjectionViewMatrix();
    uniformData.inverseProjectionViewMatrix = camera.getInverseProjectionViewMatrix();
    uniformData.cameraPosition = camera.getPosition();
    uniformData.fov = glm::radians(66.0);
    uniformData.viewportSize = {m_viewportWidth, m_viewportHeight};
    uniformData.nearPlane = Camera::NEAR;
    uniformData.farPlane = Camera::FAR;

    // Update uniform buffer data
    wgpuQueueWriteBuffer(m_Queue, uniformBuffer, 0, &uniformData, sizeof(Uniforms));

    // Create the render pass that clears the screen with our color
    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;

    // The attachment part of the render pass descriptor describes the target texture of the pass
    WGPURenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = multisampleColorTextureView;
    renderPassColorAttachment.resolveTarget = targetView; // Resolve to the target view
    renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
    renderPassColorAttachment.clearValue = WGPUColor{0.66, 0.7, 0.9, 1.0};
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = depthTextureView;
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

    Application &app = Application::GetInstance();
    auto &appData = app.getApplicationData();

    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    // Select which render pipeline to use
    wgpuRenderPassEncoderSetPipeline(renderPass, m_RenderPipeline);

    // Set the bind group
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, uniformBindGroup, 0, nullptr);

    // Set vertex buffer while encoding the render pass
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, wgpuBufferGetSize(vertexBuffer));

    // Set index buffer
    wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(indexBuffer));

    appData.renderedChunks = 0;
    appData.renderedVoxels = 0;
    for (auto &[position, chunkVertexBuffer]: m_ChunkVertexBuffers) {

        auto &[buffer, vertexCount] = chunkVertexBuffer;

        if (vertexCount == 0) {
            continue;
        }

        auto chunkCenter = glm::vec3(position) * static_cast<float>(Chunk::SIZE) + glm::vec3(Chunk::SIZE / 2);

        auto chunkSphereRadius = glm::sqrt(3.0f) * static_cast<float>(Chunk::SIZE) / 2.0f;

        if (!camera.isSphereInFrustum(chunkCenter, chunkSphereRadius)) {
            continue;
        }

        appData.renderedChunks++;
        appData.renderedVoxels += vertexCount;

        // Update uniform buffer data
        wgpuQueueWriteBuffer(m_Queue, uniformBuffer, 0, &uniformData, sizeof(Uniforms));

        // Set voxel buffer
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 1, buffer, 0, wgpuBufferGetSize(buffer) - sizeof(glm::vec4));

        // Set chunk buffer
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 2, buffer, wgpuBufferGetSize(buffer) - sizeof(glm::vec4), sizeof(glm::vec4));

        // Use instanced drawing
        wgpuRenderPassEncoderDrawIndexed(renderPass, indexCount, vertexCount, 0, 0, 0);
    }
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);
}

void RendererSystem::update(float dt) {
    World &world = GetWorld();

    auto chunks = world.getChunks();

    for (auto &chunk: chunks) {
        if (chunk.isDirty()) {
            auto position = chunk.getPosition();
            auto neighbours = world.getChunkNeighbours(position);

            if (!neighbours.hasAllNeighbours()) {
                continue;
            }

            auto bitmap = chunk.getBitmap(neighbours);

            auto buffer = createChunkVertexBuffer(position, bitmap, [&](uint32_t x, uint32_t y, uint32_t z) {
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

        m_viewportWidth = windowResizedEvent.getWidth();
        m_viewportHeight = windowResizedEvent.getHeight();

        createDepthTexture();

        Camera &camera = Application::GetInstance().getCamera();
        constexpr float fov = glm::radians(66.0);
        camera.setPerspective(fov, (float) m_viewportWidth / (float) m_viewportHeight);

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
    bgEntry.buffer = uniformBuffer;
    bgEntry.offset = 0;
    bgEntry.size = sizeof(Uniforms);

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout = bindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    uniformBindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

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
    std::array voxelAttributes{
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Uint32,
            .offset = 0,
            .shaderLocation = 1,
        },
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Uint32,
            .offset = sizeof(uint32_t),
            .shaderLocation = 2,
        },
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Uint32,
            .offset = 2 * sizeof(uint32_t),
            .shaderLocation = 3,
        },
    };

    voxelVertexBufferLayout.attributeCount = voxelAttributes.size();
    voxelVertexBufferLayout.attributes = voxelAttributes.data();
    voxelVertexBufferLayout.arrayStride = 3 * sizeof(uint32_t);
    voxelVertexBufferLayout.stepMode = WGPUVertexStepMode_Instance;

    WGPUVertexBufferLayout chunkVertexBufferLayout{};
    std::array chunkAttributes{
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Float32x4,
            .offset = 0,
            .shaderLocation = 4,
        }
    };

    chunkVertexBufferLayout.attributeCount = chunkAttributes.size();
    chunkVertexBufferLayout.attributes = chunkAttributes.data();
    chunkVertexBufferLayout.arrayStride = 0;
    chunkVertexBufferLayout.stepMode = WGPUVertexStepMode_Instance;

    std::array bufferLayouts{
        billboardVertexBufferLayout,
        voxelVertexBufferLayout,
        chunkVertexBufferLayout
    };

    std::array pipelineConstant{
        WGPUConstantEntry{
            .key = "CHUNK_SIZE",
            .value = Chunk::SIZE,
        }
    };

    pipelineDesc.vertex.bufferCount = bufferLayouts.size();
    pipelineDesc.vertex.buffers = bufferLayouts.data();
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = pipelineConstant.size();
    pipelineDesc.vertex.constants = pipelineConstant.data();

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

    if (depthTexture) {
        wgpuTextureRelease(depthTexture);
        wgpuTextureViewRelease(depthTextureView);
    }

    if (multisampleColorTexture) {
        wgpuTextureRelease(multisampleColorTexture);
        wgpuTextureViewRelease(multisampleColorTextureView);
    }

    // Create the depth texture
    WGPUTextureDescriptor depthTextureDesc = {};
    depthTextureDesc.size.width = m_viewportWidth;
    depthTextureDesc.size.height = m_viewportHeight;
    depthTextureDesc.size.depthOrArrayLayers = 1;
    depthTextureDesc.mipLevelCount = 1;
    depthTextureDesc.sampleCount = 4; // Ensure this matches the color texture sample count
    depthTextureDesc.dimension = WGPUTextureDimension_2D;
    depthTextureDesc.format = WGPUTextureFormat_Depth24PlusStencil8;
    depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;

    depthTexture = wgpuDeviceCreateTexture(device, &depthTextureDesc);
    if (!depthTexture) {
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

    depthTextureView = wgpuTextureCreateView(depthTexture, &depthViewDesc);
    if (!depthTextureView) {
        LogCore::error("Failed to create depth texture view");
        wgpuTextureRelease(depthTexture);
    }

    // Create the color texture
    WGPUTextureDescriptor colorTextureDesc = {};
    colorTextureDesc.size.width = m_viewportWidth;
    colorTextureDesc.size.height = m_viewportHeight;
    colorTextureDesc.size.depthOrArrayLayers = 1;
    colorTextureDesc.mipLevelCount = 1;
    colorTextureDesc.sampleCount = 4; // Ensure this matches the depth texture sample count
    colorTextureDesc.dimension = WGPUTextureDimension_2D;
    colorTextureDesc.format = GetWebGPUSurface().getSurfaceFormat();
    colorTextureDesc.usage = WGPUTextureUsage_RenderAttachment;

    multisampleColorTexture = wgpuDeviceCreateTexture(device, &colorTextureDesc);
    if (!multisampleColorTexture) {
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

    multisampleColorTextureView = wgpuTextureCreateView(multisampleColorTexture, &colorViewDesc);
    if (!multisampleColorTextureView) {
        LogCore::error("Failed to create color texture view");
        wgpuTextureRelease(multisampleColorTexture);
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
    std::vector<float> vertexData = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f,
        -1.0f, 1.0f,
    };
    vertexCount = static_cast<uint32_t>(vertexData.size() / 2);

    // Create vertex buffer
    WGPUBufferDescriptor bufferDesc{};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.size = vertexData.size() * sizeof(float);
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; // Vertex usage here!
    bufferDesc.mappedAtCreation = false;
    vertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

    // Upload geometry data to the buffer
    wgpuQueueWriteBuffer(m_Queue, vertexBuffer, 0, vertexData.data(), bufferDesc.size);

    // Index buffer data
    std::vector<uint16_t> indexData = {
        0, 1, 2,
        0, 2, 3,
    };
    indexCount = static_cast<uint32_t>(indexData.size());

    // Create index buffer
    WGPUBufferDescriptor indexBufferDesc{};
    indexBufferDesc.nextInChain = nullptr;
    indexBufferDesc.size = indexData.size() * sizeof(uint16_t);
    indexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
    indexBufferDesc.mappedAtCreation = false;
    indexBuffer = wgpuDeviceCreateBuffer(device, &indexBufferDesc);

    // Upload index data to the buffer
    wgpuQueueWriteBuffer(m_Queue, indexBuffer, 0, indexData.data(), indexBufferDesc.size);

    // Create uniform buffer
    WGPUBufferDescriptor uniformBufferDesc{};
    uniformBufferDesc.nextInChain = nullptr;
    uniformBufferDesc.size = sizeof(Uniforms);
    uniformBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    uniformBufferDesc.mappedAtCreation = false;
    uniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);
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

    for (uint32_t x = 1; x < BITMAP_SIZE - 1; x++) {
        for (uint32_t y = 1; y < BITMAP_SIZE - 1; y++) {
            for (uint32_t z = 1; z < BITMAP_SIZE - 1; z++) {
                const uint32_t i = x * BITMAP_SIZE * BITMAP_SIZE + y * BITMAP_SIZE + z;

                if (!bitmap.test(i)) {
                    continue;
                }

                const bool isVisible =
                        !(bitmap.test(i-1) && bitmap.test(i+1) &&
                          bitmap.test(i-BITMAP_SIZE) && bitmap.test(i+BITMAP_SIZE) &&
                          bitmap.test(i-BITMAP_SIZE*BITMAP_SIZE) && bitmap.test(i+BITMAP_SIZE*BITMAP_SIZE));

                if (isVisible) {
                    const uint8_t vx = x-1;
                    const uint8_t vy = y-1;
                    const uint8_t vz = z-1;

                    const auto& voxel = getVoxel(vx, vy, vz);

                    auto ao = AmbientOcclusion::None;

                    auto test = [&](const int dx, const int dy, const int dz) {
                        return bitmap.test((x+dx) * BITMAP_SIZE * BITMAP_SIZE + (y+dy) * BITMAP_SIZE + (z+dz));
                    };

                    // check corners
                    if (test(-1, -1, -1)) {
                        ao |= AmbientOcclusion::CornerNxNyNz;
                    }
                    if (test(-1, -1, 1)) {
                        ao |= AmbientOcclusion::CornerNxNyPz;
                    }
                    if (test(-1, 1, -1)) {
                        ao |= AmbientOcclusion::CornerNxPyNz;
                    }
                    if (test(-1, 1, 1)) {
                        ao |= AmbientOcclusion::CornerNxPyPz;
                    }
                    if (test(1, -1, -1)) {
                        ao |= AmbientOcclusion::CornerPxNyNz;
                    }
                    if (test(1, -1, 1)) {
                        ao |= AmbientOcclusion::CornerPxNyPz;
                    }
                    if (test(1, 1, -1)) {
                        ao |= AmbientOcclusion::CornerPxPyNz;
                    }
                    if (test(1, 1, 1)) {
                        ao |= AmbientOcclusion::CornerPxPyPz;
                    }

                    if (test(-1, -1, 0)) {
                        ao |= AmbientOcclusion::EdgeNxNy;
                    }
                    if (test(-1, 1, 0)) {
                        ao |= AmbientOcclusion::EdgeNxPy;
                    }
                    if (test(1, -1, 0)) {
                        ao |= AmbientOcclusion::EdgePxNy;
                    }
                    if (test(1, 1, 0)) {
                        ao |= AmbientOcclusion::EdgePxPy;
                    }
                    if (test(-1, 0, -1)) {
                        ao |= AmbientOcclusion::EdgeNxNz;
                    }
                    if (test(-1, 0, 1)) {
                        ao |= AmbientOcclusion::EdgeNxPz;
                    }
                    if (test(1, 0, -1)) {
                        ao |= AmbientOcclusion::EdgePxNz;
                    }
                    if (test(1, 0, 1)) {
                        ao |= AmbientOcclusion::EdgePxPz;
                    }
                    if (test(0, -1, -1)) {
                        ao |= AmbientOcclusion::EdgeNyNz;
                    }
                    if (test(0, -1, 1)) {
                        ao |= AmbientOcclusion::EdgeNyPz;
                    }
                    if (test(0, 1, -1)) {
                        ao |= AmbientOcclusion::EdgePyNz;
                    }
                    if (test(0, 1, 1)) {
                        ao |= AmbientOcclusion::EdgePyPz;
                    }

                    vertices.emplace_back(VertexData{vx, vy, vz, 1, voxel, ao});
                }
            }
        }
    }
}
