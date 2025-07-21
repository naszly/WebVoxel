#pragma once

#include <memory>
#include <webgpu/webgpu.h>

#include "WebGPUContext.h"

struct GLFWwindow;

class WebGpuSurface {
public:
    WebGpuSurface(GLFWwindow* window, const std::shared_ptr<WebGpuContext>& context);
    ~WebGpuSurface();

    WebGpuSurface(const WebGpuSurface&) = delete;
    WebGpuSurface(WebGpuSurface&&) = delete;
    WebGpuSurface& operator=(const WebGpuSurface&) = delete;
    WebGpuSurface& operator=(WebGpuSurface&&) = delete;

    void resize() {
        configureSurface();
    }

    [[nodiscard]] const WGPUSurface& getSurface() const {
        return m_surface;
    }

    [[nodiscard]] const WGPUTextureFormat& getSurfaceFormat() const {
        return m_surfaceFormat;
    }

    [[nodiscard]] int getWidth() const {
        return m_width;
    }

    [[nodiscard]] int getHeight() const {
        return m_height;
    }

private:
    std::shared_ptr<WebGpuContext> m_context;
    GLFWwindow* m_window{};
    int m_width{}, m_height{};

    WGPUSurface m_surface{};
    WGPUTextureFormat m_surfaceFormat{};
    WGPUPresentMode m_presentMode;

    void configureSurface();

    static WGPUTextureFormat getPreferredFormat(const WGPUSurfaceCapabilities& capabilities,
                                                std::initializer_list<WGPUTextureFormat> preferredFormats);
};
