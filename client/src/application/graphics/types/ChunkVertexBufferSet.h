#pragma once
#include "ChunkVertexBuffer.h"

struct ChunkVertexBufferSet {
    ChunkVertexBuffer fullResolution;
    ChunkVertexBuffer downsampledBy2;
    ChunkVertexBuffer downsampledBy4;
    ChunkVertexBuffer downsampledBy8;

    void clearCounts() noexcept {
        fullResolution.vertexCount = 0;
        downsampledBy2.vertexCount = 0;
        downsampledBy4.vertexCount = 0;
        downsampledBy8.vertexCount = 0;
    }

    void destroyBuffers() noexcept {
        auto releaseIf = [](WGPUBuffer& b){
            if (b) { wgpuBufferRelease(b); b = nullptr; }
        };
        releaseIf(fullResolution.buffer);
        releaseIf(downsampledBy2.buffer);
        releaseIf(downsampledBy4.buffer);
        releaseIf(downsampledBy8.buffer);
        clearCounts();
    }
};
