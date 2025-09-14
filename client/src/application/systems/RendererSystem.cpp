#include "RendererSystem.h"

#include "application/Application.h"
#include "application/domain/BlockLightInfo.h"
#include "core/events/ApplicationEvent.h"
#include "common/Log.h"
#include "common/FileSystem.h"
#include "application/graphics/PipelineBuilder.h"
#include "application/meshing/LightPropagator.h"
#include "application/meshing/AmbientOcclusionComputer.h"

void RendererSystem::initialize() {
    LogApp::info("RendererSystem::initialize");

    m_queue = wgpuDeviceGetQueue(getWebGpuContext().getDevice());

    m_viewportWidth = getWebGpuSurface().getWidth();
    m_viewportHeight = getWebGpuSurface().getHeight();

    getCamera().setAspect(static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight));

    initializeBuffers();

    m_uniformsBuffer = UniformsBuffer::make<Uniforms>(getWebGpuContext().getDevice());

    m_renderTargets = std::make_unique<RenderTargets>(getWebGpuContext());
    m_renderTargets->configure(m_viewportWidth, m_viewportHeight, m_sampleCount, getWebGpuSurface().getSurfaceFormat());

    createRenderPipeline();

    m_profiler.init(getWebGpuContext().getAdapter(), getWebGpuContext().getDevice());
}

auto RendererSystem::getChunksToRender(const Camera& camera) {
    struct SortEntry {
        float distance2; // Squared distance to camera
        ChunkVertexBuffer chunkVertexBuffer;
    };
    static std::vector<SortEntry> sortedChunks;
    sortedChunks.clear();

    const glm::vec3 camPos = camera.getPosition();
    constexpr float chunkSize = static_cast<float>(Chunk::WIDTH);
    constexpr float sqrt3 = 1.73205080757f;
    constexpr float chunkSphereRadius = sqrt3 * chunkSize * 0.5f;
    constexpr glm::vec3 chunkAabbHalfExtents = glm::vec3(chunkSize) * 0.5f;

    for (auto &[position, chunkVertexBuffer] : m_chunkVertexBuffers) {
        if (chunkVertexBuffer.vertexCount == 0) {
            continue;
        }

        const auto chunkCenter = glm::vec3(position) * chunkSize + chunkAabbHalfExtents;

        if (!camera.isSphereInFrustum(chunkCenter, chunkSphereRadius)) {
            continue;
        }

        float distance2 = glm::length2(chunkCenter - camPos);
        sortedChunks.emplace_back(distance2, chunkVertexBuffer);
    }

    std::ranges::sort(sortedChunks, [&](const auto &a, const auto &b) {
        return a.distance2 < b.distance2; // Front-to-back!
    });

    return sortedChunks | std::views::transform([](const auto &entry) -> auto & {
        return entry.chunkVertexBuffer;
    });
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

    auto chunkVertexBuffers = getChunksToRender(camera);

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
    World &world = getWorld();
    const glm::vec3 playerPosition = getCamera().getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    const auto dirtyChunks = collectDirtyChunks(world, playerChunk);
    processDirtyChunks(world, dirtyChunks);
    removeFarChunkBuffers(playerChunk);

    static float timeAccumulator = 0.0f;
    timeAccumulator += dt;
    if (timeAccumulator >= 1e+8) {
        timeAccumulator -= 1e+8;
    }
    m_uniformData.time = timeAccumulator;
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

bool RendererSystem::hasAllNeighbours(const World& world, const Chunk& chunk) {
    const auto neighbours = world.getChunkNeighborhood(chunk.getPosition());
    return neighbours.hasAllNeighbours();
}

bool RendererSystem::testBitmaps(const ChunkNeighborhood& neighborChunks, const uint32_t x, const uint32_t y, const uint32_t z) {
    assert(x < 3 * Chunk::WIDTH && y < 3 * Chunk::WIDTH && z < 3 * Chunk::WIDTH);
    const uint32_t cnx = x / Chunk::WIDTH;
    const uint32_t cny = y / Chunk::WIDTH;
    const uint32_t cnz = z / Chunk::WIDTH;
    const uint32_t bx = x % Chunk::WIDTH;
    const uint32_t by = y % Chunk::WIDTH;
    const uint32_t bz = z % Chunk::WIDTH;
    const uint32_t index = bx * Chunk::WIDTH * Chunk::WIDTH + by * Chunk::WIDTH + bz;
    return neighborChunks.getChunk(cnx,cny,cnz)->getBitmap().test(index);
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

    const auto& centerChunk = *neighborChunks.getCenterChunk();
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
    const auto& centerChunk = *neighborChunks.getCenterChunk();

    // Find all light sources in the chunk
    std::vector<Chunk::LightSource> lights;
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                const auto& chunk = *neighborChunks.getChunk(x, y, z);
                auto chunkLight = chunk.getLightSources(x-1, y-1, z-1);
                lights.insert(lights.end(), chunkLight.begin(), chunkLight.end());
            }
        }
    }

    const auto& lightMap = LightPropagator::compute(neighborChunks, lights);

    // The chunk is now in the center of a 3*WIDTH bitmap, so valid voxel region is:
    // x, y, z in [WIDTH, 2*WIDTH)
    for (uint32_t x = Chunk::WIDTH; x < 2 * Chunk::WIDTH; ++x) {
        for (uint32_t y = Chunk::WIDTH; y < 2 * Chunk::WIDTH; ++y) {
            for (uint32_t z = Chunk::WIDTH; z < 2 * Chunk::WIDTH; ++z) {
                if (testBitmaps(neighborChunks, x, y, z)) {
                    // Check if not fully surrounded
                    const bool nx = testBitmaps(neighborChunks, x-1, y, z);
                    const bool px = testBitmaps(neighborChunks, x+1, y, z);
                    const bool ny = testBitmaps(neighborChunks, x, y-1, z);
                    const bool py = testBitmaps(neighborChunks, x, y+1, z);
                    const bool nz = testBitmaps(neighborChunks, x, y, z-1);
                    const bool pz = testBitmaps(neighborChunks, x, y, z+1);
                    const bool surrounded = nx & px & ny & py & nz & pz;
                    if (!surrounded) {
                        const uint8_t vx = x - Chunk::WIDTH;
                        const uint8_t vy = y - Chunk::WIDTH;
                        const uint8_t vz = z - Chunk::WIDTH;
                        if (vx < Chunk::WIDTH && vy < Chunk::WIDTH && vz < Chunk::WIDTH) {
                            const auto& voxel = centerChunk.getVoxel(vx, vy, vz);

                            const auto ambientOcclusion = AmbientOcclusionComputer::compute(neighborChunks, x, y, z);

                            const auto& faceNxLightIntensity = lightMap.getLightInfo(x - 1, y, z);
                            const auto& facePxLightIntensity = lightMap.getLightInfo(x + 1, y, z);
                            const auto& faceNyLightIntensity = lightMap.getLightInfo(x, y - 1, z);
                            const auto& facePyLightIntensity = lightMap.getLightInfo(x, y + 1, z);
                            const auto& faceNzLightIntensity = lightMap.getLightInfo(x, y, z - 1);
                            const auto& facePzLightIntensity = lightMap.getLightInfo(x, y, z + 1);
                            const auto voxelLight = PackedLight(faceNxLightIntensity, facePxLightIntensity,
                                                                faceNyLightIntensity, facePyLightIntensity,
                                                                faceNzLightIntensity, facePzLightIntensity);

                            VertexData voxelData = {
                                vx, vy, vz, 1, voxel,
                                ambientOcclusion,
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
