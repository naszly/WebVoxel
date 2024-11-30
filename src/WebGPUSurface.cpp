#include "WebGPUSurface.h"

#include "Log.h"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

WebGPUSurface::WebGPUSurface(GLFWwindow *window, const std::shared_ptr<WebGPUContext>& context) : m_Context(context), m_Window(window) {

    m_Surface = glfwCreateWindowWGPUSurface(context->getInstance(), window);

    if (!m_Surface) {
        LogCore::critical("Failed to create WebGPU surface");
        return;
    }

    WGPUSurfaceCapabilities capabilities = {};
    wgpuSurfaceGetCapabilities(m_Surface, context->getAdapter(), &capabilities);

    m_SurfaceFormat = capabilities.formats[0];

    configureSurface();

    LogCore::info("WebGPU surface created: {0}", reinterpret_cast<size_t>(m_Surface));
}

WebGPUSurface::~WebGPUSurface() {
    auto surface = reinterpret_cast<size_t>(m_Surface);
    wgpuSurfaceRelease(m_Surface);
    LogCore::info("WebGPU surface released: {0}", surface);
}

void WebGPUSurface::configureSurface() {
    glfwGetFramebufferSize(m_Window, &m_Width, &m_Height);

    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.device = m_Context->getDevice();
    config.format = m_SurfaceFormat;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    config.width = m_Width;
    config.height = m_Height;
    config.presentMode = WGPUPresentMode_Fifo;

    wgpuSurfaceConfigure(m_Surface, &config);
}
