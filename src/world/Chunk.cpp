#include "Chunk.h"

#include "../Application.h"

void Chunk::createVertexBuffer(int x, int y, int z) {
    const auto context = Application::GetInstance().getWebGPUContext();
    const auto device = context->getDevice();

    const std::vector<VertexData> points = m_Data.getVertices(4);
    const auto chunkPosition = glm::vec4(x, y, z, 0.0f);

    WGPUBufferDescriptor descriptor{};
    descriptor.size = points.size() * sizeof(VertexData) + sizeof(chunkPosition);
    descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    descriptor.mappedAtCreation = false;

    m_VertexBuffer = wgpuDeviceCreateBuffer(device, &descriptor);
    m_VertexCount = points.size();

    const auto queue = wgpuDeviceGetQueue(device);

    wgpuQueueWriteBuffer(queue, m_VertexBuffer, 0, points.data(), points.size() * sizeof(VertexData));
    wgpuQueueWriteBuffer(queue, m_VertexBuffer, points.size() * sizeof(VertexData), &chunkPosition, sizeof(chunkPosition));
}