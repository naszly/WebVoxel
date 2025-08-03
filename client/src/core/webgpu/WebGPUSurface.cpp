#include "WebGPUSurface.h"

#include "common/Exception.h"
#include "common/Log.h"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <ranges>
#include <algorithm>

#include <magic_enum.hpp>

WebGpuSurface::WebGpuSurface(GLFWwindow *window, const std::shared_ptr<WebGpuContext>& context) : m_context(context), m_window(window) {

    m_surface = glfwCreateWindowWGPUSurface(context->getInstance(), window);

    if (!m_surface) {
        throw Exception("Failed to create WebGPU surface");
    }

    WGPUSurfaceCapabilities capabilities = {};
    wgpuSurfaceGetCapabilities(m_surface, context->getAdapter(), &capabilities);

    const auto preferredFormats = {
        WGPUTextureFormat_BGRA8Unorm, WGPUTextureFormat_RGBA8Unorm
    };

    m_surfaceFormat = getPreferredFormat(capabilities, preferredFormats);

    m_presentMode = WGPUPresentMode_Fifo;
    for (uint32_t i = 0; i < capabilities.presentModeCount; ++i) {
        if (capabilities.presentModes[i] == WGPUPresentMode_Immediate) {
            m_presentMode = WGPUPresentMode_Immediate;
            break;
        }
        if (capabilities.presentModes[i] == WGPUPresentMode_Mailbox) {
            m_presentMode = WGPUPresentMode_Mailbox;
        }
    }

    configureSurface();

    LogWebGpu::info("WebGPU surface created: {0}, format: {1}, present mode: {2}",
              reinterpret_cast<size_t>(m_surface),
              magic_enum::enum_name(m_surfaceFormat),
              magic_enum::enum_name(m_presentMode));
}

WebGpuSurface::~WebGpuSurface() {
    auto surface = reinterpret_cast<size_t>(m_surface);
    wgpuSurfaceRelease(m_surface);
    LogWebGpu::info("WebGPU surface released: {0}", surface);
}

void WebGpuSurface::configureSurface() {
    glfwGetFramebufferSize(m_window, &m_width, &m_height);

    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.device = m_context->getDevice();
    config.format = m_surfaceFormat;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    config.width = m_width;
    config.height = m_height;
    config.presentMode = m_presentMode;

    wgpuSurfaceConfigure(m_surface, &config);
}

WGPUTextureFormat WebGpuSurface::getPreferredFormat(const WGPUSurfaceCapabilities &capabilities,
                                                    const std::initializer_list<WGPUTextureFormat> preferredFormats) {
    std::vector availableFormats(capabilities.formats, capabilities.formats + capabilities.formatCount);

    auto it = std::ranges::find_first_of(availableFormats, preferredFormats);

    if (it != availableFormats.end()) {
        return *it;
    }

    return availableFormats[0];
}
