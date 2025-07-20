#include "WebGPUContext.h"

#include <magic_enum.hpp>

#include "common/Log.h"

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

void WebGPUContext::pollEvents() const {
#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(m_Device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(m_Device, false, nullptr);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
    emscripten_sleep(100);
#endif
}

void WebGPUContext::createInstance() {
    constexpr WGPUInstanceFeatureName requiredFeatures[] = {
        WGPUInstanceFeatureName_TimedWaitAny
    };
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;
    desc.requiredFeatureCount = 1;
    desc.requiredFeatures = requiredFeatures;

    m_Instance = wgpuCreateInstance(&desc);

    if (!m_Instance) {
        LogCore::critical("Failed to create WebGPU instance");
        return;
    }

    LogCore::info("WebGPU instance created: {0}", reinterpret_cast<size_t>(m_Instance));
}

void onAdapterRequest(const WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                            WGPU_NULLABLE void *userdata1, WGPU_NULLABLE void *userdata2) {
    const auto data = static_cast<WGPUAdapter*>(userdata1);

    if (status == WGPURequestAdapterStatus_Success) {
        *data = adapter;
    } else {
        LogCore::critical("Failed to request WebGPU adapter: {0}", message.data);
    }
}

void WebGPUContext::requestAdapter() {
    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.nextInChain = nullptr;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = onAdapterRequest;
    callbackInfo.userdata1 = &m_Adapter;

    constexpr WGPURequestAdapterOptions options = {};

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuInstanceRequestAdapter(m_Instance, &options, callbackInfo);

    const auto waitStatus = wgpuInstanceWaitAny(m_Instance, 1, &futureInfo, 6e+10);

    if (waitStatus != WGPUWaitStatus_Success) {
        LogCore::critical("Failed to wait for WebGPU adapter request: {0}", magic_enum::enum_name(waitStatus));
        return;
    }

    LogCore::info("WebGPU adapter requested: {0}", reinterpret_cast<size_t>(m_Adapter));
}

void onDeviceRequest(const WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message,
                     WGPU_NULLABLE void *userdata1, WGPU_NULLABLE void *userdata2) {
    const auto data = static_cast<WGPUDevice*>(userdata1);

    if (status == WGPURequestDeviceStatus_Success) {
        *data = device;
    } else {
        LogCore::critical("Failed to request WebGPU device: {0}", message.data);
    }
}

void onDeviceError(WGPUDevice const* device, const WGPUErrorType type, WGPUStringView message,
                   WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
    LogCore::error("Uncaptured device error: type {0} ({1})", magic_enum::enum_name(type), message.data);
}

void onDeviceLost(WGPUDevice const* device, const WGPUDeviceLostReason reason, WGPUStringView message,
                  WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
    LogCore::error("Device lost: reason {0} ({1})", magic_enum::enum_name(reason), message.data);
}

void WebGPUContext::requestDevice() {
    WGPUDeviceLostCallbackInfo deviceLostCallbackInfo = {};
    deviceLostCallbackInfo.nextInChain = nullptr;
    deviceLostCallbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    deviceLostCallbackInfo.callback = onDeviceLost;

    WGPUUncapturedErrorCallbackInfo uncapturedErrorCallbackInfo = {};
    uncapturedErrorCallbackInfo.nextInChain = nullptr;
    uncapturedErrorCallbackInfo.callback = onDeviceError;

    const std::vector requiredFeatures = {
        WGPUFeatureName_TimestampQuery,
    };

    WGPUDeviceDescriptor descriptor = {};
    descriptor.nextInChain = nullptr;
    descriptor.label = WGPUStringView{"WebGPU Device", WGPU_STRLEN};
    descriptor.requiredFeatureCount = static_cast<uint32_t>(requiredFeatures.size());
    descriptor.requiredFeatures = requiredFeatures.data();
    descriptor.requiredLimits = nullptr;
    descriptor.defaultQueue.nextInChain = nullptr;
    descriptor.defaultQueue.label = WGPUStringView{"WebGPU Queue", WGPU_STRLEN};
    descriptor.deviceLostCallbackInfo = deviceLostCallbackInfo;
    descriptor.uncapturedErrorCallbackInfo = uncapturedErrorCallbackInfo;

    WGPURequestDeviceCallbackInfo requestDeviceCallbackInfo = {};
    requestDeviceCallbackInfo.nextInChain = nullptr;
    requestDeviceCallbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    requestDeviceCallbackInfo.callback = onDeviceRequest;
    requestDeviceCallbackInfo.userdata1 = &m_Device;

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuAdapterRequestDevice(m_Adapter, &descriptor, requestDeviceCallbackInfo);

    const auto waitStatus = wgpuInstanceWaitAny(m_Instance, 1, &futureInfo, 6e+10);

    if (waitStatus != WGPUWaitStatus_Success) {
        LogCore::critical("Failed to wait for WebGPU device request: {0}", magic_enum::enum_name(waitStatus));
        return;
    }

    LogCore::info("WebGPU device requested: {0}", reinterpret_cast<size_t>(m_Device));
}