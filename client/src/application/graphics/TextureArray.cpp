
#include "TextureArray.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "common/FileSystem.h"
#include "common/Log.h"

bool TextureArray::loadTexturesRgba(const std::vector<const char*>& fileLocations) {
    constexpr int reqComp = 4;

    std::vector<uint8_t> textureArrayData;
    textureArrayData.reserve(m_width * m_height * m_layers * reqComp);
    for (const char* file : fileLocations) {
        const auto buffer = FileSystem::readFile(file);
        const auto bufferPtr = reinterpret_cast<const unsigned char*>(buffer.data());
        const int len = static_cast<int>(buffer.size());
        int x, y, comp;
        unsigned char* textureData = stbi_load_from_memory(bufferPtr, len, &x, &y, &comp, reqComp);

        if (!textureData) {
            LogApp::error("Texture can't be loaded from {0}", file);
            return false;
        }

        if (x != m_width || y != m_height) {
            LogApp::error("Texture width/height doesn't match buffer size {0} w{1}/{2} h{3}/{4}",
                           file, x, m_width, y, m_height);
            stbi_image_free(textureData);
            return false;
        }

        if (comp != reqComp) {
            LogApp::error("Texture {0} has unsupported channel count {1}, expected 4 (RGBA)", file, comp);
            stbi_image_free(textureData);
            return false;
        }

        textureArrayData.insert(textureArrayData.end(), textureData, textureData + (x * y * reqComp));

        stbi_image_free(textureData);
    }

    WGPUTextureDescriptor textureDesc = {};
    textureDesc.nextInChain = nullptr;
    textureDesc.label = WGPUStringView{"TextureArray", WGPU_STRLEN};
    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.size.width = m_width;
    textureDesc.size.height = m_height;
    textureDesc.size.depthOrArrayLayers = m_layers;
    textureDesc.sampleCount = 1;
    textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
    textureDesc.mipLevelCount = 1;
    textureDesc.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    m_texture = wgpuDeviceCreateTexture(m_webGpuContext.getDevice(), &textureDesc);

    WGPUTexelCopyTextureInfo imageCopyTexture = {};
    imageCopyTexture.texture = m_texture;
    imageCopyTexture.mipLevel = 0;
    imageCopyTexture.origin.x = 0;
    imageCopyTexture.origin.y = 0;
    imageCopyTexture.origin.z = 0;
    imageCopyTexture.aspect = WGPUTextureAspect_All;

    WGPUExtent3D extent = {};
    extent.width = m_width;
    extent.height = m_height;
    extent.depthOrArrayLayers = m_layers;

    WGPUTexelCopyBufferLayout texelCopyBufferLayout = {};
    texelCopyBufferLayout.offset = 0;
    texelCopyBufferLayout.bytesPerRow = m_width * reqComp;
    texelCopyBufferLayout.rowsPerImage = m_height;

    wgpuQueueWriteTexture(wgpuDeviceGetQueue(m_webGpuContext.getDevice()),
                          &imageCopyTexture,
                          textureArrayData.data(),
                          textureArrayData.size(),
                          &texelCopyBufferLayout,
                          &extent);

    WGPUTextureViewDescriptor textureViewDesc = {};
    textureViewDesc.nextInChain = nullptr;
    textureViewDesc.label = WGPUStringView{"TextureArray View", WGPU_STRLEN};
    textureViewDesc.format = WGPUTextureFormat_RGBA8Unorm;
    textureViewDesc.dimension = WGPUTextureViewDimension_2DArray;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = m_layers;
    textureViewDesc.aspect = WGPUTextureAspect_All;
    m_textureView = wgpuTextureCreateView(m_texture, &textureViewDesc);

    return true;
}

WGPUTextureView TextureArray::getTextureView() const {
    return m_textureView;
}
