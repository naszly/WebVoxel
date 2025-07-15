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
    UserData userData;

    const WGPURequestAdapterCallback adapterCallback = [](const WGPURequestAdapterStatus status,
                                                             const WGPUAdapter adapter,
                                                             WGPUStringView message,
                                                             WGPU_NULLABLE void *userdata1,
                                                             WGPU_NULLABLE void *userdata2)
    {
            const auto data = static_cast<UserData*>(userdata1);

            if (status == WGPURequestAdapterStatus_Success) {
                data->adapter = adapter;
            } else {
                LogCore::critical("Failed to request WebGPU adapter: {0}", message.data);
            }

            data->requestEnded = true;
    };

    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.nextInChain = nullptr;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = adapterCallback;
    callbackInfo.userdata1 = &userData;

    constexpr WGPURequestAdapterOptions options = {};

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuInstanceRequestAdapter(m_Instance, &options, callbackInfo);

    const auto waitStatus = wgpuInstanceWaitAny(m_Instance, 1, &futureInfo, 0);

    if (waitStatus != WGPUWaitStatus_Success) {
        LogCore::critical("Failed to wait for WebGPU adapter request: {0}", magic_enum::enum_name(waitStatus));
        return;
    }

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
    UserData userData;

    const WGPURequestDeviceCallback onDeviceRequestEnded = [](const WGPURequestDeviceStatus status,
                                                                 const WGPUDevice device,
                                                                 WGPUStringView message,
                                                                 WGPU_NULLABLE void *userdata1,
                                                                 WGPU_NULLABLE void *userdata2)
    {
        const auto data = static_cast<UserData*>(userdata1);

        if (status == WGPURequestDeviceStatus_Success) {
            data->device = device;
        } else {
            LogCore::critical("Failed to request WebGPU device: {0}", message.data);
        }

        data->requestEnded = true;
    };

    const WGPUUncapturedErrorCallback onDeviceError = [](WGPUDevice const * device,
                                                            const WGPUErrorType type,
                                                            WGPUStringView message,
                                                            WGPU_NULLABLE void* userdata1,
                                                            WGPU_NULLABLE void* userdata2)
    {
        LogCore::error("Uncaptured device error: type {0} ({1})", magic_enum::enum_name(type), message.data);
        // TODO: use proper error handling
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        exit(1);
#endif
    };

    const WGPUDeviceLostCallback deviceLostCallback = [](WGPUDevice const * device,
                                                            const WGPUDeviceLostReason reason,
                                                            WGPUStringView message,
                                                            WGPU_NULLABLE void* userdata1,
                                                            WGPU_NULLABLE void* userdata2)
    {
        LogCore::error("Device lost: reason {0} ({1})", magic_enum::enum_name(reason), message.data);
    };

    WGPUDeviceLostCallbackInfo deviceLostCallbackInfo = {};
    deviceLostCallbackInfo.nextInChain = nullptr;
    deviceLostCallbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    deviceLostCallbackInfo.callback = deviceLostCallback;

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
    requestDeviceCallbackInfo.callback = onDeviceRequestEnded;
    requestDeviceCallbackInfo.userdata1 = &userData;

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuAdapterRequestDevice(m_Adapter, &descriptor, requestDeviceCallbackInfo);

    const auto waitStatus = wgpuInstanceWaitAny(m_Instance, 1, &futureInfo, 0);

    if (waitStatus != WGPUWaitStatus_Success) {
        LogCore::critical("Failed to wait for WebGPU device request: {0}", magic_enum::enum_name(waitStatus));
        return;
    }

#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(userData.requestEnded);

    m_Device = userData.device;
    LogCore::info("WebGPU device requested: {0}", reinterpret_cast<size_t>(m_Device));
}