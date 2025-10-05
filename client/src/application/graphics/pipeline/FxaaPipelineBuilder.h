#pragma once
#include <webgpu/webgpu.h>

class FxaaPipelineBuilder {
public:
    FxaaPipelineBuilder(const WGPUDevice& device) : m_device(device) {}
    WGPURenderPipeline build(WGPUBindGroupLayout bindGroupLayout, WGPUTextureFormat surfaceFormat) const;
private:
    WGPUDevice m_device;
};

