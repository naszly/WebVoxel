#pragma once


#include <glfw3webgpu.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

#ifdef WEBGPU_BACKEND_WGPU
#include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "Log.h"

class WebGPUContext {
public:
    explicit WebGPUContext(GLFWwindow* window);
    ~WebGPUContext();

    WebGPUContext(const WebGPUContext&) = delete;
    WebGPUContext(WebGPUContext&&) = delete;
    WebGPUContext& operator=(const WebGPUContext&) = delete;
    WebGPUContext& operator=(WebGPUContext&&) = delete;

    WGPUDevice& getDevice() { return m_Device; }

    WGPUSurface& getSurface() { return m_Surface; }

    WGPUTextureFormat& getSurfaceFormat() { return m_SurfaceFormat; }

private:
    WGPUInstance m_Instance{};
    WGPUAdapter m_Adapter{};
    WGPUDevice m_Device{};
    WGPUSurface m_Surface{};
    WGPUTextureFormat m_SurfaceFormat{};

    void createInstance();
    void requestAdapter();
    void logAdapterLimits();
    void logAdapterFeatures();
    void logAdapterProperties();
    void requestDevice();
    void getSurface(GLFWwindow *window);
};
