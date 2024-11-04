#pragma once

#include <webgpu/webgpu.h>

#include "WebGPUContext.h"

struct GLFWwindow;

class WebGPUSurface {
public:
    WebGPUSurface(GLFWwindow* window, const WebGPUContext& context);
    ~WebGPUSurface();

    WebGPUSurface(const WebGPUSurface&) = delete;
    WebGPUSurface(WebGPUSurface&&) = delete;
    WebGPUSurface& operator=(const WebGPUSurface&) = delete;
    WebGPUSurface& operator=(WebGPUSurface&&) = delete;

    [[nodiscard]] const WGPUSurface& getSurface() const {
        return m_Surface;
    }

    [[nodiscard]] const WGPUTextureFormat& getSurfaceFormat() const {
        return m_SurfaceFormat;
    }

    [[nodiscard]] int getWidth() const {
        return width;
    }

    [[nodiscard]] int getHeight() const {
        return height;
    }

private:
    GLFWwindow* m_Window{};
    int width{}, height{};

    WGPUSurface m_Surface{};
    WGPUTextureFormat m_SurfaceFormat{};
};
