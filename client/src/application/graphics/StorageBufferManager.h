#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>

class StorageBufferManager {
public:
    StorageBufferManager(const WGPUDevice& device, const WGPUQueue& queue, const void* initialData, uint64_t dataSize, WGPUBufferUsage usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage);
    ~StorageBufferManager();

    [[nodiscard]] WGPUBuffer getBuffer() const { return m_buffer; }
    [[nodiscard]] uint64_t getBufferSize() const { return m_bufferSize; }

private:
    WGPUBuffer m_buffer{};
    uint64_t m_bufferSize{};
};
