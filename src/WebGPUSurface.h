#pragma once

#include <memory>
#include <webgpu/webgpu.h>

#include "WebGPUContext.h"

struct GLFWwindow;

class WebGPUSurface {
public:
    WebGPUSurface(GLFWwindow* window, const std::shared_ptr<WebGPUContext>& context);
    ~WebGPUSurface();

    WebGPUSurface(const WebGPUSurface&) = delete;
    WebGPUSurface(WebGPUSurface&&) = delete;
    WebGPUSurface& operator=(const WebGPUSurface&) = delete;
    WebGPUSurface& operator=(WebGPUSurface&&) = delete;

    void resize() {
        configureSurface();
    }

    [[nodiscard]] const WGPUSurface& getSurface() const {
        return m_Surface;
    }

    [[nodiscard]] const WGPUTextureFormat& getSurfaceFormat() const {
        return m_SurfaceFormat;
    }

    [[nodiscard]] int getWidth() const {
        return m_Width;
    }

    [[nodiscard]] int getHeight() const {
        return m_Height;
    }

private:
    std::shared_ptr<WebGPUContext> m_Context;
    GLFWwindow* m_Window{};
    int m_Width{}, m_Height{};

    WGPUSurface m_Surface{};
    WGPUTextureFormat m_SurfaceFormat{};

    void configureSurface();
};
