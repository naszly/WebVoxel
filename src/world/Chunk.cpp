#include "Chunk.h"

#include "../Application.h"
#include "../Timer.h"

void Chunk::generate() {
    const Timer timer("Chunk::generate");
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            for (int k = 0; k < SIZE; k++) {
                const uint32_t randomValue = random();
                VoxelData voxel(
                    randomValue & 0xFF,
                    (randomValue >> 8) & 0xFF,
                    (randomValue >> 16) & 0xFF
                );
                m_Data.setVoxel(i, j, k, voxel);
            }
        }
    }
}

void Chunk::createVertexBuffer(const ChunkNeighbours &chunkNeighbours) {
    const auto context = Application::GetInstance().getWebGPUContext();
    const auto device = context->getDevice();

    const auto neighbours = getNeighbours(chunkNeighbours);

    const std::vector<VertexData> points = m_Data.getVertices(neighbours);

    const auto queue = wgpuDeviceGetQueue(device);

    if (m_VertexBuffer.buffer != nullptr) {
        if (m_VertexBuffer.vertexCount == points.size()) {
            wgpuQueueWriteBuffer(queue, m_VertexBuffer.buffer, 0, points.data(), points.size() * sizeof(VertexData));
            return;
        }
    }

    const auto chunkPosition = glm::vec4(m_Position, 0.0f);

    const size_t bufferSize = points.size() * sizeof(VertexData) + sizeof(chunkPosition);

    WGPUBufferDescriptor descriptor{};
    descriptor.size = bufferSize;
    descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    descriptor.mappedAtCreation = false;

    deleteVertexBuffer();

    m_VertexBuffer.buffer = wgpuDeviceCreateBuffer(device, &descriptor);
    m_VertexBuffer.vertexCount = points.size();

    std::vector<uint8_t> data(bufferSize);
    memcpy(data.data(), points.data(), points.size() * sizeof(VertexData));
    memcpy(data.data() + points.size() * sizeof(VertexData), &chunkPosition, sizeof(chunkPosition));

    wgpuQueueWriteBuffer(queue, m_VertexBuffer.buffer, 0, data.data(), data.size());
}

void Chunk::deleteVertexBuffer() {
    if (m_VertexBuffer.buffer != nullptr) {
        wgpuBufferDestroy(m_VertexBuffer.buffer);
        m_VertexBuffer.buffer = nullptr;
    }
}

Chunk::SparseVoxelOctTree::Neighbours Chunk::getNeighbours(const ChunkNeighbours &chunkNeighbours) {
    return {
        .xMinus = chunkNeighbours.xMinus ? &chunkNeighbours.xMinus->m_Data : nullptr,
        .xPlus = chunkNeighbours.xPlus ? &chunkNeighbours.xPlus->m_Data : nullptr,
        .yMinus = chunkNeighbours.yMinus ? &chunkNeighbours.yMinus->m_Data : nullptr,
        .yPlus = chunkNeighbours.yPlus ? &chunkNeighbours.yPlus->m_Data : nullptr,
        .zMinus = chunkNeighbours.zMinus ? &chunkNeighbours.zMinus->m_Data : nullptr,
        .zPlus = chunkNeighbours.zPlus ? &chunkNeighbours.zPlus->m_Data : nullptr
    };
}
