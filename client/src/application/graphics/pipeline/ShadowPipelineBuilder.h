#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>

class ShadowPipelineBuilder {
public:
    struct Artifacts {
        WGPURenderPipeline pipeline{};
        WGPUBindGroup bindGroup{};
    };

    ShadowPipelineBuilder() = delete;

    [[nodiscard]] static Artifacts build(const WGPUDevice& device,
                                         const WGPUBuffer& shadowUniformBuffer,
                                         size_t chunkSize);
};

