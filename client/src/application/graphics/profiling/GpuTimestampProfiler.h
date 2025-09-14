#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>

class GpuTimestampProfiler {
public:
    void init(const WGPUAdapter& adapter, const WGPUDevice& device);
    bool isSupported() const { return m_supported; }

    // Attach timestamp writes to a render pass descriptor (no-op if unsupported)
    void attachTo(WGPURenderPassDescriptor& desc) const;

    // Resolve queries and copy to CPU-visible buffer (no-op if unsupported or mapped)
    void resolveAndCopy(const WGPUCommandEncoder& encoder) const;

    // Block until submitted work done and export durations to a file (no-op if unsupported)
    void exportTimestamps(const WGPUInstance& instance, const WGPUQueue& queue) const;

private:
    // Internal mapping + file write
    void exportTimestampsInternal(const WGPUInstance& instance) const;

    bool m_supported = false;

    WGPUQuerySet m_querySet{};
    WGPUBuffer m_queryResolveBuffer{};
    WGPUBuffer m_queryReadBuffer{};

    // Circular write into read buffer
    uint64_t m_queryReadBufferCapacity = 524288ull * sizeof(uint64_t);
    mutable uint64_t m_queryReadBufferSize = 0;

    // Cached struct used when attaching to passes
    WGPUPassTimestampWrites m_timestampWrites{};
};

