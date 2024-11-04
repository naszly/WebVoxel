#include "WebGPUContext.h"

#include "Log.h"

WebGPUContext::WebGPUContext() {
    createInstance();
    requestAdapter();
    logAdapterLimits();
    logAdapterFeatures();
    logAdapterProperties();
    requestDevice();
}

WebGPUContext::~WebGPUContext() {
    wgpuInstanceRelease(m_Instance);
    wgpuAdapterRelease(m_Adapter);
    wgpuDeviceRelease(m_Device);
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

    LogCore::info("WebGPU instance created: {0}", (size_t)m_Instance);
}

void WebGPUContext::requestAdapter() {
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *userData) {
        auto data = static_cast<UserData*>(userData);

        if (status == WGPURequestAdapterStatus_Success) {
            data->adapter = adapter;
        } else {
            LogCore::critical("Failed to request WebGPU adapter: {0}", message);
        }

        data->requestEnded = true;
    };

    const WGPURequestAdapterOptions options = {};

    wgpuInstanceRequestAdapter(m_Instance, &options, onAdapterRequestEnded, &userData);

#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(userData.requestEnded);

    m_Adapter = userData.adapter;
    LogCore::info("WebGPU adapter requested: {0}", (size_t)m_Adapter);
}

void WebGPUContext::logAdapterLimits() {

#ifndef __EMSCRIPTEN__
    WGPUSupportedLimits supportedLimits = {};
    supportedLimits.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
    bool success = wgpuAdapterGetLimits(m_Adapter, &supportedLimits) == WGPUStatus_Success;
#else
    bool success = wgpuAdapterGetLimits(m_Adapter, &supportedLimits);
#endif

    if (!success) {
        LogCore::error("Failed to get WebGPU adapter limits");
        return;
    }

    LogCore::info("WebGPU adapter limits:");
    LogCore::info("  maxTextureDimension1D: {0}", supportedLimits.limits.maxTextureDimension1D);
    LogCore::info("  maxTextureDimension2D: {0}", supportedLimits.limits.maxTextureDimension2D);
    LogCore::info("  maxTextureDimension3D: {0}", supportedLimits.limits.maxTextureDimension3D);
    LogCore::info("  maxTextureArrayLayers: {0}", supportedLimits.limits.maxTextureArrayLayers);
    LogCore::info("  maxBindGroups: {0}", supportedLimits.limits.maxBindGroups);
    LogCore::info("  maxBindGroupsPlusVertexBuffers: {0}", supportedLimits.limits.maxBindGroupsPlusVertexBuffers);
    LogCore::info("  maxBindingsPerBindGroup: {0}", supportedLimits.limits.maxBindingsPerBindGroup);
    LogCore::info("  maxDynamicUniformBuffersPerPipelineLayout: {0}", supportedLimits.limits.maxDynamicUniformBuffersPerPipelineLayout);
    LogCore::info("  maxDynamicStorageBuffersPerPipelineLayout: {0}", supportedLimits.limits.maxDynamicStorageBuffersPerPipelineLayout);
    LogCore::info("  maxSampledTexturesPerShaderStage: {0}", supportedLimits.limits.maxSampledTexturesPerShaderStage);
    LogCore::info("  maxSamplersPerShaderStage: {0}", supportedLimits.limits.maxSamplersPerShaderStage);
    LogCore::info("  maxStorageBuffersPerShaderStage: {0}", supportedLimits.limits.maxStorageBuffersPerShaderStage);
    LogCore::info("  maxStorageTexturesPerShaderStage: {0}", supportedLimits.limits.maxStorageTexturesPerShaderStage);
    LogCore::info("  maxUniformBuffersPerShaderStage: {0}", supportedLimits.limits.maxUniformBuffersPerShaderStage);
    LogCore::info("  maxUniformBufferBindingSize: {0}", supportedLimits.limits.maxUniformBufferBindingSize);
    LogCore::info("  maxStorageBufferBindingSize: {0}", supportedLimits.limits.maxStorageBufferBindingSize);
    LogCore::info("  minUniformBufferOffsetAlignment: {0}", supportedLimits.limits.minUniformBufferOffsetAlignment);
    LogCore::info("  minStorageBufferOffsetAlignment: {0}", supportedLimits.limits.minStorageBufferOffsetAlignment);
    LogCore::info("  maxVertexBuffers: {0}", supportedLimits.limits.maxVertexBuffers);
    LogCore::info("  maxBufferSize: {0}", supportedLimits.limits.maxBufferSize);
    LogCore::info("  maxVertexAttributes: {0}", supportedLimits.limits.maxVertexAttributes);
    LogCore::info("  maxVertexBufferArrayStride: {0}", supportedLimits.limits.maxVertexBufferArrayStride);
    LogCore::info("  maxInterStageShaderComponents: {0}", supportedLimits.limits.maxInterStageShaderComponents);
    LogCore::info("  maxInterStageShaderVariables: {0}", supportedLimits.limits.maxInterStageShaderVariables);
    LogCore::info("  maxColorAttachments: {0}", supportedLimits.limits.maxColorAttachments);
    LogCore::info("  maxColorAttachmentBytesPerSample: {0}", supportedLimits.limits.maxColorAttachmentBytesPerSample);
    LogCore::info("  maxComputeWorkgroupStorageSize: {0}", supportedLimits.limits.maxComputeWorkgroupStorageSize);
    LogCore::info("  maxComputeInvocationsPerWorkgroup: {0}", supportedLimits.limits.maxComputeInvocationsPerWorkgroup);
    LogCore::info("  maxComputeWorkgroupSizeX: {0}", supportedLimits.limits.maxComputeWorkgroupSizeX);
    LogCore::info("  maxComputeWorkgroupSizeY: {0}", supportedLimits.limits.maxComputeWorkgroupSizeY);
    LogCore::info("  maxComputeWorkgroupSizeZ: {0}", supportedLimits.limits.maxComputeWorkgroupSizeZ);
    LogCore::info("  maxComputeWorkgroupsPerDimension: {0}", supportedLimits.limits.maxComputeWorkgroupsPerDimension);

#endif // NOT __EMSCRIPTEN__
}

void WebGPUContext::logAdapterFeatures() {
#ifndef __EMSCRIPTEN__
    std::vector<WGPUFeatureName> features;
    size_t featureCount = wgpuAdapterEnumerateFeatures(m_Adapter, nullptr);

    features.resize(featureCount);

    wgpuAdapterEnumerateFeatures(m_Adapter, features.data());

    LogCore::info("WebGPU adapter features:");

    for (const auto& feature : features) {
        LogCore::info("  {0:#010x}", (uint64_t)feature);
    }
#endif // NOT __EMSCRIPTEN__
}

void WebGPUContext::logAdapterProperties() {
#ifndef __EMSCRIPTEN__
    WGPUAdapterProperties properties = {};
    wgpuAdapterGetProperties(m_Adapter, &properties);

    LogCore::info("WebGPU adapter properties:");
    LogCore::info("  name: {0}", properties.name ? properties.name : "null");
    LogCore::info("  vendorID: {0}", properties.vendorID);
    LogCore::info("  vendorName: {0}", properties.vendorName ? properties.vendorName : "null");
    LogCore::info("  architecture: {0}", properties.architecture ? properties.architecture : "null");
    LogCore::info("  deviceID: {0}", properties.deviceID);
    LogCore::info("  driverDescription: {0}", properties.driverDescription ? properties.driverDescription : "null");
    LogCore::info("  adapterType: {0:#010x}", (uint64_t)properties.adapterType);
    LogCore::info("  backendType: {0:#010x}", (uint64_t)properties.backendType);
#endif // NOT __EMSCRIPTEN__
}

void WebGPUContext::requestDevice() {
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *userData) {
        auto data = static_cast<UserData*>(userData);

        if (status == WGPURequestDeviceStatus_Success) {
            data->device = device;
        } else {
            LogCore::critical("Failed to request WebGPU device: {0}", message);
        }

        data->requestEnded = true;
    };

    WGPUDeviceDescriptor descriptor = {};
    descriptor.nextInChain = nullptr;
    descriptor.label = "WebGPU Device";
    //descriptor.requiredFeaturesCount = 0;
    descriptor.requiredFeatures = nullptr;
    descriptor.requiredLimits = nullptr;
    descriptor.defaultQueue.nextInChain = nullptr;
    descriptor.defaultQueue.label = "WebGPU Queue";
    descriptor.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* userData) {
        LogCore::error("Device lost: reason {0:#010x}", (uint64_t)reason);
        if (message) {
            LogCore::error(" ({0})", message);
        }
    };

    wgpuAdapterRequestDevice(m_Adapter, &descriptor, onDeviceRequestEnded, &userData);

#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(userData.requestEnded);

    m_Device = userData.device;
    LogCore::info("WebGPU device requested: {0}", (size_t)m_Device);

    auto onDeviceError = [](WGPUErrorType type, char const* message, void* userData) {
        LogCore::error("Uncaptured device error: type {0:#010x}", (uint64_t)type);
        if (message) {
            LogCore::error(" ({0})", message);
        }
        // TODO: use proper error handling
#ifdef __EMSCRIPTEN__
        emscripten_force_exit(1);
#else
        exit(1);
#endif
    };
    wgpuDeviceSetUncapturedErrorCallback(m_Device, onDeviceError, nullptr);
}