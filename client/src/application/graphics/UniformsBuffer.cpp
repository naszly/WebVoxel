#include "UniformsBuffer.h"

#include <cassert>

UniformsBuffer::UniformsBuffer(const WGPUDevice& device, const uint64_t byteSize) : m_size(byteSize) {
    WGPUBufferDescriptor desc{};
    desc.size = byteSize;
    desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    desc.mappedAtCreation = false;
    m_buffer = wgpuDeviceCreateBuffer(device, &desc);
}

UniformsBuffer::~UniformsBuffer() {
    if (m_buffer) {
        wgpuBufferRelease(m_buffer);
        m_buffer = nullptr;
    }
}

void UniformsBuffer::write(const WGPUQueue& queue, const void* data, const uint64_t byteSize) const {
    assert(byteSize <= m_size);
    wgpuQueueWriteBuffer(queue, m_buffer, 0, data, byteSize);
}

