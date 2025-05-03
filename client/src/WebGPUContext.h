#pragma once

#include <webgpu/webgpu.h>

#ifdef WEBGPU_BACKEND_WGPU
#include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

class WebGPUContext {
public:
    WebGPUContext();
    ~WebGPUContext();

    WebGPUContext(const WebGPUContext&) = delete;
    WebGPUContext(WebGPUContext&&) = delete;
    WebGPUContext& operator=(const WebGPUContext&) = delete;
    WebGPUContext& operator=(WebGPUContext&&) = delete;

    [[nodiscard]] const WGPUInstance& getInstance() const { return m_Instance; }

    [[nodiscard]] const WGPUAdapter& getAdapter() const { return m_Adapter; }

    [[nodiscard]] const WGPUDevice& getDevice() const { return m_Device; }

    void pollEvents() const;

private:
    WGPUInstance m_Instance{};
    WGPUAdapter m_Adapter{};
    WGPUDevice m_Device{};

    void createInstance();
    void requestAdapter();
    void requestDevice();
};
