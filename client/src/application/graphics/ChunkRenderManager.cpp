#include "ChunkRenderManager.h"

#include <algorithm>
#include <ranges>
#include <cstring>

#include "application/meshing/VoxelVertexGenerator.h"
#include "application/world/chunk/Chunk.h"
#include "common/Utils.h"

ChunkRenderManager::~ChunkRenderManager() {
    for (auto& [buffer, vertexCount] : m_chunkVertexBuffers | std::views::values) {
        if (buffer) {
            wgpuBufferRelease(buffer);
            buffer = nullptr;
        }
    }
}

std::vector<ChunkVertexBuffer> ChunkRenderManager::getChunksToRender(const Camera &camera) const {
    struct SortEntry { float distance2; ChunkVertexBuffer vb; };
    std::vector<SortEntry> sorted;
    sorted.reserve(m_chunkVertexBuffers.size());

    const glm::vec3 camPos = camera.getPosition();
    constexpr float chunkSize = static_cast<float>(Chunk::WIDTH);
    constexpr float sqrt3 = 1.73205080757f;
    constexpr float chunkSphereRadius = sqrt3 * chunkSize * 0.5f;
    constexpr glm::vec3 chunkAabbHalfExtents = glm::vec3(chunkSize) * 0.5f;

    for (const auto &[chunkPos, vb] : m_chunkVertexBuffers) {
        if (vb.vertexCount == 0) {
            continue;
        }

        const auto chunkCenter = glm::vec3(chunkPos) * chunkSize + chunkAabbHalfExtents;
        if (!camera.isSphereInFrustum(chunkCenter, chunkSphereRadius)) {
            continue;
        }

        float distance2 = glm::length2(chunkCenter - camPos);
        sorted.emplace_back(distance2, vb);
    }

    std::ranges::sort(sorted, [](const auto &a, const auto &b){ return a.distance2 < b.distance2; });

    std::vector<ChunkVertexBuffer> result;
    result.reserve(sorted.size());
    for (auto& [distance2, vb] : sorted) result.emplace_back(vb);
    return result;
}

void ChunkRenderManager::update(const Camera& camera) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    updateDirtyChunks(playerChunk);
    removeBuffersOfFarChunks(playerChunk);
}

void ChunkRenderManager::updateDirtyChunks(const glm::ivec3 &playerChunk) {
    const auto dirtyChunks = collectDirtyChunks(playerChunk);
    processDirtyChunks(dirtyChunks);
}

void ChunkRenderManager::removeBuffersOfFarChunks(const glm::ivec3 &playerChunk) {
    for (auto it = m_chunkVertexBuffers.begin(); it != m_chunkVertexBuffers.end();) {
        const auto &[chunkPosition, vertexBuffer] = *it;
        const uint64_t distance2 = Utils::distance2(chunkPosition, playerChunk);
        constexpr auto farPlane = Camera::FAR / Chunk::WIDTH;
        if (distance2 > static_cast<uint64_t>(farPlane * farPlane)) {
            wgpuBufferRelease(vertexBuffer.buffer);
            m_chunkVertexBuffers.erase(it++);
        } else {
            ++it;
        }
    }
}

std::vector<ChunkRenderManager::ChunkRef> ChunkRenderManager::collectDirtyChunks(const glm::ivec3 &playerChunk) const {
    const auto& chunks = m_world.getChunks();
    std::vector<ChunkRef> dirty;

    std::ranges::copy_if(chunks, std::back_inserter(dirty), [&](const Chunk &chunk) {
        if (!chunk.isGpuBufferDirty()) {
            return false;
        }
        const auto neighborhood = m_world.getChunkNeighborhood(chunk.getPosition());
        return neighborhood.hasAllNeighbours();
    });

    std::ranges::sort(dirty, [&](const Chunk &a, const Chunk &b) {
        const auto aPos = a.getPosition();
        const auto bPos = b.getPosition();
        return Utils::distance2(aPos, playerChunk) < Utils::distance2(bPos, playerChunk);
    });

    return dirty;
}

void ChunkRenderManager::processDirtyChunks(const std::vector<ChunkRef> &dirtyChunks) {
    const size_t maxChunksToProcess = 6 + dirtyChunks.size() / 8;
    size_t processed = 0;

    for (auto &chunkRef : dirtyChunks) {
        if (processed >= maxChunksToProcess) {
            break;
        }

        auto &chunk = chunkRef.get();
        auto position = chunk.getPosition();
        auto chunkNeighborhood = m_world.getChunkNeighborhood(position);
        if (!chunkNeighborhood.hasAllNeighbours()) {
            continue;
        }

        ChunkVertexBuffer buffer = createChunkVertexBuffer(chunkNeighborhood);

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
        ++processed;
    }
}

ChunkVertexBuffer ChunkRenderManager::createChunkVertexBuffer(const ChunkNeighborhood &neighborChunks) const {
    ChunkVertexBuffer vertexBuffer;

    thread_local std::vector<VertexData> points;
    points.clear();

    VoxelVertexGenerator::generate(neighborChunks, points);

    if (points.empty()) {
        return vertexBuffer;
    }

    const auto queue = wgpuDeviceGetQueue(m_device);

    const auto &centerChunk = *neighborChunks.getCenterChunk();
    const auto chunkPosition = glm::vec4(centerChunk.getPosition(), 0.0f);

    const size_t bufferSize = points.size() * sizeof(VertexData) + sizeof(chunkPosition);

    WGPUBufferDescriptor descriptor{};
    descriptor.size = bufferSize;
    descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    descriptor.mappedAtCreation = false;
    descriptor.label = WGPUStringView{"Chunk Vertex Buffer", WGPU_STRLEN};

    vertexBuffer.buffer = wgpuDeviceCreateBuffer(m_device, &descriptor);
    vertexBuffer.vertexCount = points.size();

    thread_local std::vector<uint8_t> data;
    if (data.size() < bufferSize) {
        data.resize(bufferSize);
    }

    std::memcpy(data.data(), points.data(), points.size() * sizeof(VertexData));
    std::memcpy(data.data() + points.size() * sizeof(VertexData), &chunkPosition, sizeof(chunkPosition));

    wgpuQueueWriteBuffer(queue, vertexBuffer.buffer, 0, data.data(), bufferSize);

    return vertexBuffer;
}
