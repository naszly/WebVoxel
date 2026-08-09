#include "RendererSystem.h"

#include "application/Application.h"
#include "application/graphics/pipeline/FxaaPipelineBuilder.h"
#include "application/graphics/pipeline/PipelineBuilder.h"
#include "application/meshing/ChunkVertexData.h"
#include "core/events/ApplicationEvent.h"
#include "common/Log.h"
#include "application/types/VertexData.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace {
struct RemotePlayerUpdate {
    glm::vec3 position;
    glm::vec3 direction;
};
std::mutex remotePlayerUpdatesMutex;
std::unordered_map<std::string, RemotePlayerUpdate> remotePlayerUpdates;
std::vector<std::string> removedRemotePlayers;
}

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE void updateRemotePlayer(
    const char* playerId, const double x, const double y, const double z,
    const double directionX, const double directionY, const double directionZ) {
    if (!playerId) return;
    std::scoped_lock lock(remotePlayerUpdatesMutex);
    remotePlayerUpdates[playerId] = {
        glm::vec3(x, y, z), glm::vec3(directionX, directionY, directionZ)
    };
}

extern "C" EMSCRIPTEN_KEEPALIVE void removeRemotePlayer(const char* playerId) {
    if (!playerId) return;
    std::scoped_lock lock(remotePlayerUpdatesMutex);
    remotePlayerUpdates.erase(playerId);
    removedRemotePlayers.emplace_back(playerId);
}
#endif

RendererSystem::~RendererSystem() {
    if (m_remotePlayerMetadataBuffer) wgpuBufferRelease(m_remotePlayerMetadataBuffer);
    if (m_remotePlayerVertexBuffer) wgpuBufferRelease(m_remotePlayerVertexBuffer);
    if (m_remotePlayerUniformBindGroup) wgpuBindGroupRelease(m_remotePlayerUniformBindGroup);
    if (m_remotePlayerPipeline) wgpuRenderPipelineRelease(m_remotePlayerPipeline);
}

void RendererSystem::initialize() {
    LogApp::info("RendererSystem::initialize");

    auto& gpuContext = getWebGpuContext();
    auto& device = gpuContext.getDevice();

    m_queue = wgpuDeviceGetQueue(device);

    m_viewportWidth = getWebGpuSurface().getWidth();
    m_viewportHeight = getWebGpuSurface().getHeight();

    getCamera().setAspect(static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight));

    m_uniformsBuffer = UniformsBuffer::make<Uniforms>(device);

    m_renderTargets = std::make_unique<RenderTargets>(gpuContext);
    m_renderTargets->configure(m_viewportWidth, m_viewportHeight, getWebGpuSurface().getSurfaceFormat());

    m_chunkRenderManager = std::make_unique<ChunkRenderManager>(device, getWorld());

    createPipelines();

    m_profiler.init(gpuContext.getAdapter(), device);
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

    if (m_lighting && m_shadows) {
        auto& nearShadowPass = m_shadowCascades[static_cast<size_t>(ShadowCascade::Near)];
        auto& farShadowPass = m_shadowCascades[static_cast<size_t>(ShadowCascade::Far)];
        if (nearShadowPass) {
            renderShadowPass(encoder,*nearShadowPass,chooseResolutionByDistance);
        }
        if (farShadowPass) {
            renderShadowPass(encoder,*farShadowPass,chooseResolutionByDistance);
        }
    }

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

    auto& appData = getApplicationData();
    appData.renderedChunks = 0;
    appData.renderedVoxels = 0;

    const auto sceneChunkVertexBuffers = m_chunkRenderManager->getChunksToRender(getCamera(), chooseResolutionByDistance);
    for (auto &chunkVertexBuffer: sceneChunkVertexBuffers) {
        auto &[buffer, vertexCount] = chunkVertexBuffer;

        appData.renderedChunks++;
        appData.renderedVoxels += vertexCount;

        const uint64_t chunkMetaOffset = vertexCount * sizeof(VertexData);

        wgpuRenderPassEncoderSetVertexBuffer(scenePass, 0, buffer, 0, chunkMetaOffset);
        wgpuRenderPassEncoderSetVertexBuffer(scenePass, 1, buffer, chunkMetaOffset, sizeof(glm::vec4));

        wgpuRenderPassEncoderDraw(scenePass, 6, vertexCount, 0, 0);
    }
    renderRemotePlayers(scenePass);
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
    wgpuRenderPassEncoderDraw(fxaaPass, 6, 1, 0, 0);

    wgpuRenderPassEncoderEnd(fxaaPass);
    wgpuRenderPassEncoderRelease(fxaaPass);

    m_profiler.resolveAndCopy(encoder);
}

void RendererSystem::update(const float dt) {
    const Camera& camera = getCamera();
    m_chunkRenderManager->removeBuffersOfFarChunks(camera);
    updateRemotePlayerBuffers(dt);

    m_timeAccumulator = std::fmod(m_timeAccumulator + dt, 1e+8f);
}

void RendererSystem::updateRemotePlayerBuffers(const float dt) {
    std::unordered_map<std::string, RemotePlayerUpdate> updates;
    std::vector<std::string> removals;
    {
        std::scoped_lock lock(remotePlayerUpdatesMutex);
        updates.swap(remotePlayerUpdates);
        removals.swap(removedRemotePlayers);
    }

    for (const auto& playerId : removals) {
        if (const auto found = m_remotePlayers.find(playerId); found != m_remotePlayers.end()) {
            m_remotePlayers.erase(found);
            m_remotePlayersDirty = true;
        }
    }

    for (const auto& [playerId, update] : updates) {
        auto [iterator, inserted] = m_remotePlayers.try_emplace(playerId);
        auto& player = iterator->second;
        player.targetPosition = update.position;
        const glm::vec3 horizontalDirection(update.direction.x, 0.0f, update.direction.z);
        if (glm::dot(horizontalDirection, horizontalDirection) > 0.000001f) {
            player.targetDirection = glm::normalize(horizontalDirection);
        }
        if (inserted) {
            player.position = update.position;
            player.direction = player.targetDirection;
        }
        m_remotePlayersDirty = true;
    }

    const float interpolation = std::clamp(dt * 12.0f, 0.0f, 1.0f);
    const auto device = getWebGpuContext().getDevice();
    for (auto& [playerId, player] : m_remotePlayers) {
        const glm::vec3 positionDelta = player.targetPosition - player.position;
        if (glm::dot(positionDelta, positionDelta) > 0.000001f) {
            player.position = glm::mix(player.position, player.targetPosition, interpolation);
            if (const glm::vec3 remaining = player.targetPosition - player.position;
                glm::dot(remaining, remaining) <= 0.000001f)
                player.position = player.targetPosition;
            m_remotePlayersDirty = true;
        }
        constexpr float twoPi = 6.28318530717958647692f;
        const float currentAngle = std::atan2(player.direction.x, player.direction.z);
        const float targetAngle = std::atan2(player.targetDirection.x, player.targetDirection.z);
        const float angleDelta = std::remainder(targetAngle - currentAngle, twoPi);
        if (std::abs(angleDelta) > 0.001f) {
            const float nextAngle = currentAngle + angleDelta * interpolation;
            player.direction = glm::vec3(std::sin(nextAngle), 0.0f, std::cos(nextAngle));
            if (std::abs(angleDelta) * (1.0f - interpolation) <= 0.001f)
                player.direction = player.targetDirection;
            m_remotePlayersDirty = true;
        }
    }

    if (!m_remotePlayersDirty) return;

    std::vector<VertexData> vertices;
    std::vector<glm::vec4> metadata;
    vertices.reserve(m_remotePlayers.size());
    metadata.reserve(m_remotePlayers.size());
    for (const auto& [playerId, player] : m_remotePlayers) {

        const auto hash = std::hash<std::string>{}(playerId);
        const auto color = VoxelData(
            static_cast<uint8_t>(64 + (hash & 127)),
            static_cast<uint8_t>(64 + ((hash >> 8) & 127)),
            static_cast<uint8_t>(64 + ((hash >> 16) & 127)));
        vertices.emplace_back(VertexData{0, 0, 0, 1, color, {}, {}});
        const float facingAngle = std::atan2(player.direction.x, player.direction.z);
        const glm::vec4 playerOffset(
            player.position.x,
            player.position.y,
            player.position.z,
            facingAngle + 10.0f);
        metadata.push_back(playerOffset);
    }

    const uint64_t vertexBytes = vertices.size() * sizeof(VertexData);
    const uint64_t metadataBytes = metadata.size() * sizeof(glm::vec4);
    const auto ensureBuffer = [&](WGPUBuffer& buffer, uint64_t& allocatedSize, const uint64_t requiredSize,
                                  const char* label) {
        if (requiredSize == 0 || allocatedSize >= requiredSize) return;
        if (buffer) wgpuBufferRelease(buffer);
        allocatedSize = std::max<uint64_t>(requiredSize, allocatedSize * 2);
        WGPUBufferDescriptor descriptor{};
        descriptor.size = allocatedSize;
        descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
        descriptor.label = WGPUStringView{label, WGPU_STRLEN};
        buffer = wgpuDeviceCreateBuffer(device, &descriptor);
    };
    ensureBuffer(m_remotePlayerVertexBuffer, m_remotePlayerVertexBufferSize, vertexBytes, "Remote Player Vertices");
    ensureBuffer(m_remotePlayerMetadataBuffer, m_remotePlayerMetadataBufferSize, metadataBytes, "Remote Player Metadata");
    if (vertexBytes > 0) {
        wgpuQueueWriteBuffer(m_queue, m_remotePlayerVertexBuffer, 0, vertices.data(), vertexBytes);
        wgpuQueueWriteBuffer(m_queue, m_remotePlayerMetadataBuffer, 0, metadata.data(), metadataBytes);
    }
    m_remotePlayerInstanceCount = static_cast<uint32_t>(vertices.size());
    m_remotePlayersDirty = false;
}

void RendererSystem::renderRemotePlayers(const WGPURenderPassEncoder scenePass) const {
    if (m_remotePlayerInstanceCount == 0) return;
    wgpuRenderPassEncoderSetPipeline(scenePass, m_remotePlayerPipeline);
    wgpuRenderPassEncoderSetBindGroup(scenePass, 0, m_remotePlayerUniformBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(scenePass, 0, m_remotePlayerVertexBuffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(scenePass, 1, m_remotePlayerMetadataBuffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDraw(scenePass, 48, m_remotePlayerInstanceCount, 0, 0);
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

void RendererSystem::setShadows(const bool shadows) {
    if (m_shadows != shadows) {
        m_shadows = shadows;
        createPipelines();
    }
}

void RendererSystem::exportTimestamps() const {
    const auto queue = wgpuDeviceGetQueue(getWebGpuContext().getDevice());
    const WGPUInstance& instance = getWebGpuContext().getInstance();
    m_profiler.exportTimestamps(instance, queue);
}

void RendererSystem::updateChunkVertexBuffer(const ChunkVertexData& vertexData, const glm::vec3& chunkPosition) const {
    m_chunkRenderManager->updateChunkVertexBuffer(vertexData, chunkPosition);
}

void RendererSystem::createPipelines() {
    const auto& device = getWebGpuContext().getDevice();
    m_shadowCascades[static_cast<size_t>(ShadowCascade::Near)] = std::make_unique<ShadowPass>(device, Chunk::WIDTH, 2048);
    m_shadowCascades[static_cast<size_t>(ShadowCascade::Far)]  = std::make_unique<ShadowPass>(device, Chunk::WIDTH, 2048);

    if (!m_shadowSampler) {
        WGPUSamplerDescriptor samplerDesc{};
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
        m_shadows,
        m_sampleCount,
        Chunk::WIDTH,
        getWebGpuSurface().getSurfaceFormat()
    };

    const auto& uniformBuffer = m_uniformsBuffer->get();
    const auto& blockTextureManager = getBlockTextureManager();
    const auto& nearCascade = m_shadowCascades[static_cast<size_t>(ShadowCascade::Near)];
    const auto& farCascade  = m_shadowCascades[static_cast<size_t>(ShadowCascade::Far)];
    const auto artifacts = builder.build(
        opts,
        uniformBuffer,
        blockTextureManager,
        nearCascade->getDepthView(),
        farCascade->getDepthView(),
        m_shadowSampler
    );
    const auto remotePlayerArtifacts = builder.build(
        opts,
        uniformBuffer,
        blockTextureManager,
        nearCascade->getDepthView(),
        farCascade->getDepthView(),
        m_shadowSampler,
        true
    );

    if (m_renderPipeline) wgpuRenderPipelineRelease(m_renderPipeline);
    if (m_uniformBindGroup) wgpuBindGroupRelease(m_uniformBindGroup);
    if (m_remotePlayerPipeline) wgpuRenderPipelineRelease(m_remotePlayerPipeline);
    if (m_remotePlayerUniformBindGroup) wgpuBindGroupRelease(m_remotePlayerUniformBindGroup);
    m_renderPipeline = artifacts.pipeline;
    m_uniformBindGroup = artifacts.uniformBindGroup;
    m_remotePlayerPipeline = remotePlayerArtifacts.pipeline;
    m_remotePlayerUniformBindGroup = remotePlayerArtifacts.uniformBindGroup;
}

void RendererSystem::updateUniformBuffer() const {
    if (!m_uniformsBuffer) return;

    constexpr double orthoHalfNear = 120.0;
    constexpr double orthoHalfFar = 800.0;

    const Camera& camera = getCamera();

    const glm::dvec3 lightDir = glm::normalize(glm::vec3(2.0, -7.0, 3.0));

    const glm::mat4 lightProjectionViewNear =
        ShadowPass::computeDirectionalLightProjectionView(orthoHalfNear, -orthoHalfNear, orthoHalfNear, lightDir);

    const glm::mat4 lightProjectionViewFar =
        ShadowPass::computeDirectionalLightProjectionView(orthoHalfFar,  -orthoHalfFar, orthoHalfFar, lightDir);

    const Uniforms uniforms{
        .projectionViewMatrix = camera.getProjectionViewMatrix(),
        .inverseProjectionViewMatrix = camera.getInverseProjectionViewMatrix(),
        .cameraPosition = camera.getPosition(),
        .fov = camera.getFov(),
        .viewportSize = glm::vec2(static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight)),
        .nearPlane = Camera::NEAR,
        .farPlane = Camera::FAR,
        .cameraDir = camera.getDirection(),
        .time = m_timeAccumulator,
        .lightProjectionViewMatrixNear = lightProjectionViewNear,
        .lightProjectionViewMatrixFar = lightProjectionViewFar,
        .lightDirection = lightDir
    };
    m_uniformsBuffer->write(m_queue, uniforms);

    auto updateCascade = [&](ShadowCascade idx, const glm::mat4& lightProjectionView, const float orthoHalf) {
        const auto& pass = m_shadowCascades[static_cast<size_t>(idx)];
        if (!pass) return;
        const float mapSize = static_cast<float>(pass->getSize());
        pass->update(
            m_queue,
            lightProjectionView,
            camera.getPosition(),
            glm::vec2(mapSize, mapSize),
            -orthoHalf,
            orthoHalf,
            glm::vec3(lightDir),
            m_timeAccumulator
        );
    };
    updateCascade(ShadowCascade::Near, lightProjectionViewNear, orthoHalfNear);
    updateCascade(ShadowCascade::Far,  lightProjectionViewFar, orthoHalfFar);
}

void RendererSystem::renderShadowPass(const WGPUCommandEncoder &encoder, const ShadowPass &shadowPass,
                                      const ChunkRenderManager::ChooseResolutionFunc& chooseResolution) const {
    WGPURenderPassDepthStencilAttachment shadowDepthAttachment = {};
    shadowDepthAttachment.view = shadowPass.getDepthView();
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

    WGPURenderPassEncoder shadow = wgpuCommandEncoderBeginRenderPass(encoder, &shadowPassDesc);
    wgpuRenderPassEncoderSetPipeline(shadow, shadowPass.getPipeline());
    wgpuRenderPassEncoderSetBindGroup(shadow, 0, shadowPass.getBindGroup(), 0, nullptr);

    const auto shadowChunkVertexBuffers = m_chunkRenderManager->getChunksToRender(
        getCamera().getPosition(),
        shadowPass.getLightProjectionView(),
        chooseResolution
    );
    for (auto &chunkVertexBuffer: shadowChunkVertexBuffers) {
        auto &[buffer, vertexCount] = chunkVertexBuffer;
        const uint64_t chunkMetaOffset = vertexCount * sizeof(VertexData);
        wgpuRenderPassEncoderSetVertexBuffer(shadow, 0, buffer, 0, chunkMetaOffset);
        wgpuRenderPassEncoderSetVertexBuffer(shadow, 1, buffer, chunkMetaOffset, sizeof(glm::vec4));
        wgpuRenderPassEncoderDraw(shadow, 6, vertexCount, 0, 0);
    }

    if (m_remotePlayerInstanceCount > 0) {
        wgpuRenderPassEncoderSetPipeline(shadow, shadowPass.getRemotePlayerPipeline());
        wgpuRenderPassEncoderSetVertexBuffer(shadow, 0, m_remotePlayerVertexBuffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetVertexBuffer(shadow, 1, m_remotePlayerMetadataBuffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderDraw(shadow, 48, m_remotePlayerInstanceCount, 0, 0);
    }

    wgpuRenderPassEncoderEnd(shadow);
    wgpuRenderPassEncoderRelease(shadow);
}

const ChunkVertexBuffer& RendererSystem::chooseResolutionByDistance(const ChunkVertexBufferSet& set,
                                                                    const float distance) {
    if (distance < 600.0f) {
        return set.fullResolution;
    }
    if (distance < 800.0f) {
        return set.downsampledBy2;
    }
    if (distance < 1200.0f) {
        return set.downsampledBy4;
    }
    return set.downsampledBy4;
}
