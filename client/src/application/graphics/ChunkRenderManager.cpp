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

void ChunkRenderManager::removeBuffersOfFarChunks(const Camera& camera) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

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

void ChunkRenderManager::updateChunkVertexBuffer(const std::vector<VertexData>& points, const glm::vec3 &position) {
    ChunkVertexBuffer buffer = createChunkVertexBuffer(points, position);

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
}

ChunkVertexBuffer ChunkRenderManager::createChunkVertexBuffer(const std::vector<VertexData>& points, const glm::vec3 &position) const {
    ChunkVertexBuffer vertexBuffer;

    if (points.empty()) {
        return vertexBuffer;
    }

    const auto queue = wgpuDeviceGetQueue(m_device);

    const auto chunkPosition = glm::vec4(position, 0.0f);

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
