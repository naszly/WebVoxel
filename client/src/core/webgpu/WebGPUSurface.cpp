#include "WebGPUSurface.h"

#include "common/Log.h"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <ranges>
#include <algorithm>

#include <magic_enum.hpp>

WebGPUSurface::WebGPUSurface(GLFWwindow *window, const std::shared_ptr<WebGPUContext>& context) : m_Context(context), m_Window(window) {

    m_Surface = glfwCreateWindowWGPUSurface(context->getInstance(), window);

    if (!m_Surface) {
        LogCore::critical("Failed to create WebGPU surface");
        return;
    }

    WGPUSurfaceCapabilities capabilities = {};
    wgpuSurfaceGetCapabilities(m_Surface, context->getAdapter(), &capabilities);

    const auto preferredFormats = {
        WGPUTextureFormat_BGRA8Unorm, WGPUTextureFormat_RGBA8Unorm
    };

    m_SurfaceFormat = getPreferredFormat(capabilities, preferredFormats);

    m_PresentMode = WGPUPresentMode_Fifo;
    for (uint32_t i = 0; i < capabilities.presentModeCount; ++i) {
        if (capabilities.presentModes[i] == WGPUPresentMode_Immediate) {
            m_PresentMode = WGPUPresentMode_Immediate;
            break;
        }
        if (capabilities.presentModes[i] == WGPUPresentMode_Mailbox) {
            m_PresentMode = WGPUPresentMode_Mailbox;
        }
    }

    configureSurface();

    LogCore::info("WebGPU surface created: {0}, format: {1}, present mode: {2}",
              reinterpret_cast<size_t>(m_Surface),
              magic_enum::enum_name(m_SurfaceFormat),
              magic_enum::enum_name(m_PresentMode));
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
    config.presentMode = m_PresentMode;

    wgpuSurfaceConfigure(m_Surface, &config);
}

WGPUTextureFormat WebGPUSurface::getPreferredFormat(const WGPUSurfaceCapabilities &capabilities,
                                                    const std::initializer_list<WGPUTextureFormat> preferredFormats) {
    std::vector availableFormats(capabilities.formats, capabilities.formats + capabilities.formatCount);

    auto it = std::ranges::find_first_of(availableFormats, preferredFormats);

    if (it != availableFormats.end()) {
        return *it;
    }

    return availableFormats[0];
}
