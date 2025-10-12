#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>

class BlockTextureManager;

class PipelineOptions {
public:
    PipelineOptions() = delete;
    PipelineOptions(const bool lighting,
                    const bool fog,
                    const bool pointLight,
                    const int sampleCount,
                    const size_t chunkSize,
                    const WGPUTextureFormat colorFormat)
        : m_lighting(lighting),
          m_fog(fog),
          m_pointLight(pointLight),
          m_sampleCount(sampleCount),
          m_chunkSize(chunkSize),
          m_colorFormat(colorFormat) {}

    [[nodiscard]] double lighting() const { return m_lighting; }
    [[nodiscard]] double fog() const { return m_fog; }
    [[nodiscard]] double pointLight() const { return m_pointLight; }
    [[nodiscard]] double sampleCount() const { return m_sampleCount; }
    [[nodiscard]] double chunkSize() const { return m_chunkSize; }
    [[nodiscard]] WGPUTextureFormat colorFormat() const { return m_colorFormat; }

private:
    bool m_lighting{};
    bool m_fog{};
    bool m_pointLight{};
    int m_sampleCount{};
    size_t m_chunkSize{};
    WGPUTextureFormat m_colorFormat{};
};

struct PipelineArtifacts {
    WGPURenderPipeline pipeline{};
    WGPUBindGroup uniformBindGroup{};
};

class PipelineBuilder {
public:
    explicit PipelineBuilder(const WGPUDevice& device) : m_device(device) {}

    [[nodiscard]] PipelineArtifacts build(const PipelineOptions& options,
                                          const WGPUBuffer& uniformBuffer,
                                          const BlockTextureManager& blockTextures,
                                          const WGPUTextureView& shadowMapView,
                                          const WGPUSampler& shadowSampler) const;

private:
    WGPUDevice m_device{};
};
