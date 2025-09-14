#include "StorageBuffer.h"

StorageBuffer::StorageBuffer(const WGPUDevice& device, const WGPUQueue& queue, const void* initialData, const uint64_t dataSize, const WGPUBufferUsage usage) {
    m_bufferSize = dataSize;
    WGPUBufferDescriptor bufferDesc{};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.size = m_bufferSize;
    bufferDesc.usage = usage;
    bufferDesc.mappedAtCreation = false;
    m_buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
    if (initialData && m_bufferSize > 0) {
        wgpuQueueWriteBuffer(queue, m_buffer, 0, initialData, m_bufferSize);
    }
}

StorageBuffer::~StorageBuffer() {
    if (m_buffer) {
        wgpuBufferRelease(m_buffer);
    }
}
