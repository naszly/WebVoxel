#include "Chunk.h"

#include "../Application.h"

void Chunk::generate(int x, int y, int z) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            for (int k = 0; k < SIZE; k++) {
                VoxelData data{};
                data.r = random() % 255;
                data.g = random() % 255;
                data.b = random() % 255;
                data.a = 255;

                m_Data.set(i, j, k, data);
            }
        }
    }
}

void Chunk::createVertexBuffer(const int x, const int y, const int z) {
    const auto context = Application::GetInstance().getWebGPUContext();
    const auto device = context->getDevice();

    const std::vector<VertexData> points = m_Data.getVertices();
    const auto chunkPosition = glm::vec4(x, y, z, 0.0f);

    WGPUBufferDescriptor descriptor{};
    descriptor.size = points.size() * sizeof(VertexData) + sizeof(chunkPosition);
    descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    descriptor.mappedAtCreation = false;

    deleteVertexBuffer();

    m_VertexBuffer.buffer = wgpuDeviceCreateBuffer(device, &descriptor);
    m_VertexBuffer.vertexCount = points.size();

    const auto queue = wgpuDeviceGetQueue(device);

    wgpuQueueWriteBuffer(queue, m_VertexBuffer.buffer, 0, points.data(), points.size() * sizeof(VertexData));
    wgpuQueueWriteBuffer(queue, m_VertexBuffer.buffer, points.size() * sizeof(VertexData), &chunkPosition, sizeof(chunkPosition));
}

void Chunk::deleteVertexBuffer() {
    if (m_VertexBuffer.buffer != nullptr) {
        wgpuBufferDestroy(m_VertexBuffer.buffer);
        m_VertexBuffer.buffer = nullptr;
    }
}
