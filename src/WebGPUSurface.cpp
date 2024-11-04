//
// Created by kornel on 21/10/24.
//

#include "WebGPUSurface.h"

#include "Log.h"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

WebGPUSurface::WebGPUSurface(GLFWwindow *window, const WebGPUContext& context) {

    m_Surface = glfwCreateWindowWGPUSurface(context.getInstance(), window);

    if (!m_Surface) {
        LogCore::critical("Failed to create WebGPU surface");
        return;
    }

    glfwGetFramebufferSize(window, &width, &height);

    // Configure the surface
    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;

    // Configuration of the textures created for the underlying swap chain
    config.width = width;
    config.height = height;
    config.usage = WGPUTextureUsage_RenderAttachment;
    m_SurfaceFormat = wgpuSurfaceGetPreferredFormat(m_Surface, context.getAdapter());
    config.format = m_SurfaceFormat;

    // And we do not need any particular view format:
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.device = context.getDevice();
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(m_Surface, &config);

    LogCore::info("WebGPU surface created: {0}", reinterpret_cast<size_t>(m_Surface));
}

WebGPUSurface::~WebGPUSurface() {
    size_t surface = reinterpret_cast<size_t>(m_Surface);
    wgpuSurfaceRelease(m_Surface);
    LogCore::info("WebGPU surface released: {0}", surface);
}
