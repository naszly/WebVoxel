#pragma once

#include <webgpu/webgpu.h>

struct ChunkVertexBuffer {
    WGPUBuffer buffer{nullptr};
    size_t vertexCount{0};
};
