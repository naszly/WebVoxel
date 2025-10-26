#include "ChunkRenderManager.h"

#include <algorithm>
#include <ranges>
#include <cstring>

#include "application/world/chunk/Chunk.h"
#include "common/Utils.h"

ChunkRenderManager::~ChunkRenderManager() {
    for (auto& buffer : m_chunkVertexBufferSets | std::views::values) {
        buffer.destroyBuffers();
    }
}

std::vector<ChunkVertexBuffer> ChunkRenderManager::getChunksToRender(const Camera& camera,
                                                                     const ChooseResolutionFunc& chooseResolution) const {
    const glm::vec3 cameraPosition = camera.getPosition();
    const glm::mat4 projView = camera.getProjectionViewMatrix();
    return getChunksToRender(cameraPosition, projView, chooseResolution);
}

std::vector<ChunkVertexBuffer> ChunkRenderManager::getChunksToRender(const glm::vec3& cameraPosition,
                                                                     const glm::mat4& projView,
                                                                     const ChooseResolutionFunc& chooseResolution) const {
    struct SortEntry { float distance; ChunkVertexBuffer vb; };
    std::vector<SortEntry> sorted;
    sorted.reserve(m_chunkVertexBufferSets.size());

    constexpr float chunkSize = static_cast<float>(Chunk::WIDTH);
    constexpr float sqrt3 = 1.73205080757f;
    constexpr float chunkSphereRadius = sqrt3 * chunkSize * 0.5f;
    constexpr glm::vec3 chunkAabbHalfExtents = glm::vec3(chunkSize) * 0.5f;

    for (const auto &[chunkPos, vb] : m_chunkVertexBufferSets) {
        const auto chunkCenter = glm::vec3(chunkPos) * chunkSize + chunkAabbHalfExtents;
        float distance = glm::length(chunkCenter - cameraPosition);
        auto& chosenVb = chooseResolution(vb, distance);

        if (chosenVb.vertexCount == 0) {
            continue;
        }

        if (!isSphereInFrustum(chunkCenter, chunkSphereRadius, cameraPosition, projView)) {
            continue;
        }

        sorted.emplace_back(distance, chosenVb);
    }

    std::ranges::sort(sorted, [](const auto &a, const auto &b){ return a.distance < b.distance; });

    std::vector<ChunkVertexBuffer> result;
    result.reserve(sorted.size());
    for (auto& [distance, vb] : sorted) result.emplace_back(vb);
    return result;
}


void ChunkRenderManager::removeBuffersOfFarChunks(const Camera& camera) {
    const glm::vec3 playerPosition = camera.getPosition();
    const glm::ivec3 playerChunk = WorldCoordinate(playerPosition).chunkPosition();

    for (auto it = m_chunkVertexBufferSets.begin(); it != m_chunkVertexBufferSets.end();) {
        auto &[chunkPosition, vertexBuffer] = *it;
        const uint64_t distance2 = Utils::distance2(chunkPosition, playerChunk);
        constexpr auto farPlane = Camera::FAR / Chunk::WIDTH;
        if (distance2 > static_cast<uint64_t>(farPlane * farPlane)) {
            vertexBuffer.destroyBuffers();
            m_chunkVertexBufferSets.erase(it++);
        } else {
            ++it;
        }
    }
}

void ChunkRenderManager::updateChunkVertexBuffer(const ChunkVertexData& vertexData, const glm::vec3 &position) {
    ChunkVertexBufferSet buffer {
        .fullResolution = createChunkVertexBuffer(vertexData.fullResolution, position),
        .downsampledBy2 = createChunkVertexBuffer(vertexData.downsampledBy2, position),
        .downsampledBy4 = createChunkVertexBuffer(vertexData.downsampledBy4, position),
        .downsampledBy8 = createChunkVertexBuffer(vertexData.downsampledBy8, position)
    };

    const auto it = m_chunkVertexBufferSets.find(position);
    if (it != m_chunkVertexBufferSets.end()) {
        it->second.destroyBuffers();
        if (buffer.fullResolution.vertexCount > 0) {
            it->second = buffer;
        } else {
            m_chunkVertexBufferSets.erase(it);
        }
    } else if (buffer.fullResolution.vertexCount > 0) {
        m_chunkVertexBufferSets.insert({position, buffer});
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

bool ChunkRenderManager::isSphereInFrustum(const glm::vec3& center,
                                           const float radius,
                                           const glm::vec3 cameraPosition,
                                           glm::mat4 projView) {

    // Extract frustum planes from the projection-view matrix
    std::array planes{
        glm::vec4(projView[0][3] + projView[0][2], projView[1][3] + projView[1][2], projView[2][3] + projView[2][2], projView[3][3] + projView[3][2]), // Near
        glm::vec4(projView[0][3] - projView[0][2], projView[1][3] - projView[1][2], projView[2][3] - projView[2][2], projView[3][3] - projView[3][2]), // Far
        glm::vec4(projView[0][3] + projView[0][0], projView[1][3] + projView[1][0], projView[2][3] + projView[2][0], projView[3][3] + projView[3][0]), // Left
        glm::vec4(projView[0][3] - projView[0][0], projView[1][3] - projView[1][0], projView[2][3] - projView[2][0], projView[3][3] - projView[3][0]), // Right
        glm::vec4(projView[0][3] - projView[0][1], projView[1][3] - projView[1][1], projView[2][3] - projView[2][1], projView[3][3] - projView[3][1]), // Top
        glm::vec4(projView[0][3] + projView[0][1], projView[1][3] + projView[1][1], projView[2][3] + projView[2][1], projView[3][3] + projView[3][1]), // Bottom
    };

    // Normalize the planes
    for (auto& plane : planes) {
        const float length = glm::length(glm::vec3(plane));
        plane /= length;
    }

    // Sphere-Frustum Intersection Test
    return std::ranges::all_of(planes, [&](const glm::vec4& plane) {
        const float distance = glm::dot(glm::vec3(plane), center - cameraPosition) + plane.w;
        return distance >= -radius;
    });
}
