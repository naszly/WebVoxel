#pragma once

#include <webgpu/webgpu.h>
#include <vector>

#include "core/webgpu/WebGPUContext.h"

class TextureArray {
public:
    TextureArray(const WebGpuContext& webGpuContext, const uint32_t width, const uint32_t height, const uint32_t layers)
        : m_width(width), m_height(height), m_layers(layers), m_webGpuContext(webGpuContext) {}

    ~TextureArray() {
        if (m_texture) {
            wgpuTextureDestroy(m_texture);
            wgpuTextureRelease(m_texture);
        }
    }

    bool loadTexturesRgba(const std::vector<const char*>& fileLocations);

    [[nodiscard]] WGPUTextureView getTextureView() const;

private:
    uint32_t m_width, m_height, m_layers;
    const WebGpuContext& m_webGpuContext;
    WGPUTexture m_texture{nullptr};
    WGPUTextureView m_textureView{nullptr};
};
