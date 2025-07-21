#include "WebGPUContext.h"

#include <magic_enum.hpp>

#include "common/Exception.h"
#include "common/Log.h"

WebGpuContext::WebGpuContext() {
    createInstance();
    requestAdapter();
    requestDevice();
}

WebGpuContext::~WebGpuContext() {
    wgpuDeviceRelease(m_device);
    wgpuAdapterRelease(m_adapter);
    wgpuInstanceRelease(m_instance);
}

void WebGpuContext::createInstance() {
    constexpr WGPUInstanceFeatureName requiredFeatures[] = {
        WGPUInstanceFeatureName_TimedWaitAny
    };
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;
    desc.requiredFeatureCount = 1;
    desc.requiredFeatures = requiredFeatures;

    m_instance = wgpuCreateInstance(&desc);

    if (!m_instance) {
        throw Exception("Failed to create WebGPU instance");
    }

    LogCore::info("WebGPU instance created: {0}", reinterpret_cast<size_t>(m_instance));
}

void onAdapterRequest(const WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                            WGPU_NULLABLE void *userdata1, WGPU_NULLABLE void *userdata2) {
    const auto data = static_cast<WGPUAdapter*>(userdata1);

    if (status == WGPURequestAdapterStatus_Success) {
        *data = adapter;
    } else {
        throw Exception("Failed to request WebGPU adapter: {0}", message.data);
    }
}

void WebGpuContext::requestAdapter() {
    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.nextInChain = nullptr;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = onAdapterRequest;
    callbackInfo.userdata1 = &m_adapter;

    constexpr WGPURequestAdapterOptions options = {};

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuInstanceRequestAdapter(m_instance, &options, callbackInfo);

    const auto waitStatus = wgpuInstanceWaitAny(m_instance, 1, &futureInfo, 6e+10);

    if (waitStatus != WGPUWaitStatus_Success) {
        throw Exception("Failed to wait for WebGPU adapter request: {0}", magic_enum::enum_name(waitStatus));
    }

    LogCore::info("WebGPU adapter requested: {0}", reinterpret_cast<size_t>(m_adapter));
}

void onDeviceRequest(const WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message,
                     WGPU_NULLABLE void *userdata1, WGPU_NULLABLE void *userdata2) {
    const auto data = static_cast<WGPUDevice*>(userdata1);

    if (status == WGPURequestDeviceStatus_Success) {
        *data = device;
    } else {
        throw Exception("Failed to request WebGPU device: {0}", message.data);
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

void WebGpuContext::requestDevice() {
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
    requestDeviceCallbackInfo.userdata1 = &m_device;

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuAdapterRequestDevice(m_adapter, &descriptor, requestDeviceCallbackInfo);

    const auto waitStatus = wgpuInstanceWaitAny(m_instance, 1, &futureInfo, 6e+10);

    if (waitStatus != WGPUWaitStatus_Success) {
        throw Exception("Failed to wait for WebGPU device request: {0}", magic_enum::enum_name(waitStatus));
    }

    LogCore::info("WebGPU device requested: {0}", reinterpret_cast<size_t>(m_device));
}