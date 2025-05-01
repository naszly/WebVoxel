#include "WebGPUContext.h"

#include <magic_enum.hpp>

#include "Log.h"

WebGPUContext::WebGPUContext() {
    createInstance();
    requestAdapter();
    requestDevice();
}

WebGPUContext::~WebGPUContext() {
    wgpuDeviceRelease(m_Device);
    wgpuAdapterRelease(m_Adapter);
    wgpuInstanceRelease(m_Instance);
}

void WebGPUContext::createInstance() {
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_EMSCRIPTEN
    m_Instance = wgpuCreateInstance(nullptr);
#else //  WEBGPU_BACKEND_EMSCRIPTEN
    m_Instance = wgpuCreateInstance(&desc);
#endif //  WEBGPU_BACKEND_EMSCRIPTEN

    if (!m_Instance) {
        LogCore::critical("Failed to create WebGPU instance");
        return;
    }

    LogCore::info("WebGPU instance created: {0}", reinterpret_cast<size_t>(m_Instance));
}

void WebGPUContext::requestAdapter() {
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    };

    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *userData) {
        const auto data = static_cast<UserData*>(userData);

        if (status == WGPURequestAdapterStatus_Success) {
            data->adapter = adapter;
        } else {
            LogCore::critical("Failed to request WebGPU adapter: {0}", message);
        }

        data->requestEnded = true;
    };

    constexpr WGPURequestAdapterOptions options = {};

    UserData userData;
    wgpuInstanceRequestAdapter(m_Instance, &options, onAdapterRequestEnded, &userData);

#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(userData.requestEnded);

    m_Adapter = userData.adapter;
    LogCore::info("WebGPU adapter requested: {0}", reinterpret_cast<size_t>(m_Adapter));
}

void WebGPUContext::requestDevice() {
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *userData) {
        const auto data = static_cast<UserData*>(userData);

        if (status == WGPURequestDeviceStatus_Success) {
            data->device = device;
        } else {
            LogCore::critical("Failed to request WebGPU device: {0}", message);
        }

        data->requestEnded = true;
    };

    auto onDeviceError = [](WGPUErrorType type, char const* message, void* userData) {
        LogCore::error("Uncaptured device error: type {0} ({1})", magic_enum::enum_name(type), message);
        // TODO: use proper error handling
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        exit(1);
#endif
    };

#ifdef WEBGPU_BACKEND_DAWN
    WGPUDeviceLostCallbackInfo deviceLostCallbackInfo = {};
    deviceLostCallbackInfo.nextInChain = nullptr;
    deviceLostCallbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    deviceLostCallbackInfo.callback = [](const WGPUDevice* device, const WGPUDeviceLostReason reason, char const* message, void* userData) {
        LogCore::error("Device lost: reason {0} ({1})", magic_enum::enum_name(reason), message);
    };

    WGPUUncapturedErrorCallbackInfo uncapturedErrorCallbackInfo = {};
    uncapturedErrorCallbackInfo.nextInChain = nullptr;
    uncapturedErrorCallbackInfo.callback = onDeviceError;
#endif

    WGPUDeviceDescriptor descriptor = {};
    descriptor.nextInChain = nullptr;
    descriptor.label = "WebGPU Device";
    descriptor.requiredFeatureCount = 0;
    descriptor.requiredFeatures = nullptr;
    descriptor.requiredLimits = nullptr;
    descriptor.defaultQueue.nextInChain = nullptr;
    descriptor.defaultQueue.label = "WebGPU Queue";
#ifdef WEBGPU_BACKEND_DAWN
    descriptor.deviceLostCallbackInfo = deviceLostCallbackInfo;
    descriptor.uncapturedErrorCallbackInfo = uncapturedErrorCallbackInfo;
#else
    descriptor.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* userData) {
        LogCore::error("Device lost: reason {0} ({1})", magic_enum::enum_name(reason), message);
    };
#endif

    UserData userData;
    wgpuAdapterRequestDevice(m_Adapter, &descriptor, onDeviceRequestEnded, &userData);

#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(userData.requestEnded);

    m_Device = userData.device;
    LogCore::info("WebGPU device requested: {0}", reinterpret_cast<size_t>(m_Device));

#ifndef WEBGPU_BACKEND_DAWN
    wgpuDeviceSetUncapturedErrorCallback(m_Device, onDeviceError, nullptr);
#endif
}