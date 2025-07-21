#pragma once

#include <webgpu/webgpu.h>

#ifdef WEBGPU_BACKEND_WGPU
#include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

class WebGpuContext {
public:
    WebGpuContext();
    ~WebGpuContext();

    WebGpuContext(const WebGpuContext&) = delete;
    WebGpuContext(WebGpuContext&&) = delete;
    WebGpuContext& operator=(const WebGpuContext&) = delete;
    WebGpuContext& operator=(WebGpuContext&&) = delete;

    [[nodiscard]] const WGPUInstance& getInstance() const { return m_instance; }

    [[nodiscard]] const WGPUAdapter& getAdapter() const { return m_adapter; }

    [[nodiscard]] const WGPUDevice& getDevice() const { return m_device; }

private:
    WGPUInstance m_instance{};
    WGPUAdapter m_adapter{};
    WGPUDevice m_device{};

    void createInstance();
    void requestAdapter();
    void requestDevice();
};
