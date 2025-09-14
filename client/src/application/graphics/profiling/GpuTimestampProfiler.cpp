#include "GpuTimestampProfiler.h"

#include "common/Log.h"
#include "common/FileSystem.h"
#include <magic_enum.hpp>
#include <chrono>
#include <sstream>

void GpuTimestampProfiler::init(const WGPUAdapter& adapter, const WGPUDevice& device) {
    m_supported = wgpuAdapterHasFeature(adapter, WGPUFeatureName_TimestampQuery);
    if (!m_supported) {
        LogApp::warning("TimestampQuery feature not supported");
        return;
    }

    // Query set (two timestamps per pass)
    WGPUQuerySetDescriptor querySetDesc = {};
    querySetDesc.count = 2;
    querySetDesc.type = WGPUQueryType_Timestamp;
    m_querySet = wgpuDeviceCreateQuerySet(device, &querySetDesc);

    // GPU-only resolve buffer (holds two u64 timestamps)
    WGPUBufferDescriptor resolveBufferDesc = {};
    resolveBufferDesc.size = 2 * sizeof(uint64_t);
    resolveBufferDesc.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
    resolveBufferDesc.mappedAtCreation = false;
    resolveBufferDesc.label = WGPUStringView{"Query Resolve Buffer", WGPU_STRLEN};
    m_queryResolveBuffer = wgpuDeviceCreateBuffer(device, &resolveBufferDesc);

    // CPU-visible read buffer (ring buffer of timestamp pairs)
    WGPUBufferDescriptor readBufferDesc = {};
    readBufferDesc.size = m_queryReadBufferCapacity;
    readBufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    readBufferDesc.mappedAtCreation = false;
    readBufferDesc.label = WGPUStringView{"Query Read Buffer", WGPU_STRLEN};
    m_queryReadBuffer = wgpuDeviceCreateBuffer(device, &readBufferDesc);

    // Preconfigure timestamp writes struct
    m_timestampWrites = {};
    m_timestampWrites.querySet = m_querySet;
    m_timestampWrites.beginningOfPassWriteIndex = 0;
    m_timestampWrites.endOfPassWriteIndex = 1;
}

void GpuTimestampProfiler::attachTo(WGPURenderPassDescriptor& desc) const {
    if (!m_supported) return;
    desc.timestampWrites = &m_timestampWrites;
}

void GpuTimestampProfiler::resolveAndCopy(const WGPUCommandEncoder& encoder) const {
    if (!m_supported) return;

    if (wgpuBufferGetMapState(m_queryReadBuffer) == WGPUBufferMapState_Unmapped) {
        constexpr uint64_t bufferSize = 2 * sizeof(uint64_t);

        wgpuCommandEncoderResolveQuerySet(encoder, m_querySet, 0, 2, m_queryResolveBuffer, 0);

        if (m_queryReadBufferSize >= m_queryReadBufferCapacity) {
            m_queryReadBufferSize = 0;
        }

        m_queryReadBufferSize += bufferSize;

        wgpuCommandEncoderCopyBufferToBuffer(encoder,
                                             m_queryResolveBuffer,
                                             0,
                                             m_queryReadBuffer,
                                             m_queryReadBufferSize - bufferSize,
                                             bufferSize);
    } else {
        LogApp::warning("Query read buffer is mapped, skipping copy");
    }
}

void GpuTimestampProfiler::exportTimestamps(const WGPUInstance& instance, const WGPUQueue& queue) const {
    if (!m_supported) return;

    struct CallbackData { const GpuTimestampProfiler* self; const WGPUInstance* instance; };
    CallbackData data{this, &instance};

    WGPUQueueWorkDoneCallback onWorkDoneCallback = [](WGPUQueueWorkDoneStatus status, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
        auto* d = static_cast<CallbackData*>(userdata1);
        if (status == WGPUQueueWorkDoneStatus_Success) {
            LogApp::info("Exporting timestamps...");
            d->self->exportTimestampsInternal(*d->instance);
        } else {
            LogApp::error("Failed to export timestamps");
        }
    };

    WGPUQueueWorkDoneCallbackInfo workDoneInfo = {};
    workDoneInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    workDoneInfo.callback = onWorkDoneCallback;
    workDoneInfo.userdata1 = &data;
    workDoneInfo.userdata2 = nullptr;
    workDoneInfo.nextInChain = nullptr;

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuQueueOnSubmittedWorkDone(queue, workDoneInfo);

    const auto waitStatus = wgpuInstanceWaitAny(instance, 1, &futureInfo, 6e+10);

    if (waitStatus != WGPUWaitStatus_Success) {
        LogApp::error("Failed to wait for work done: {}", magic_enum::enum_name(waitStatus));
    }
}

void GpuTimestampProfiler::exportTimestampsInternal(const WGPUInstance& instance) const {
    struct UserData {
        WGPUBuffer buffer;
        uint64_t size;
        std::vector<uint64_t> durations;
    };

    auto callback = [](const WGPUMapAsyncStatus status,
                       WGPUStringView message,
                       WGPU_NULLABLE void* userdata1,
                       WGPU_NULLABLE void* userdata2)
    {
        const auto data = static_cast<UserData*>(userdata1);
        if (status == WGPUMapAsyncStatus_Success) {
            if (const auto* timestamps = static_cast<const uint64_t*>(wgpuBufferGetConstMappedRange(data->buffer, 0, data->size))) {
                for (size_t i = 0; i < data->size / sizeof(uint64_t) / 2; ++i) {
                    uint64_t duration = timestamps[i*2+1] - timestamps[i*2];
                    data->durations.push_back(duration);
                }
            } else {
                LogApp::error("Failed to get mapped range from query read buffer");
            }
            wgpuBufferUnmap(data->buffer);
        } else {
            LogApp::error("Failed to map query read buffer: {}", magic_enum::enum_name(status));
        }
    };

    UserData userData{m_queryReadBuffer, m_queryReadBufferSize};

    auto callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = callback;
    callbackInfo.userdata1 = &userData;

    WGPUFutureWaitInfo futureInfo = {};
    futureInfo.future = wgpuBufferMapAsync(m_queryReadBuffer, WGPUMapMode_Read, 0, m_queryReadBufferCapacity, callbackInfo);

    const auto waitStatus = wgpuInstanceWaitAny(instance, 1, &futureInfo, 6e+10);

    if (waitStatus != WGPUWaitStatus_Success) {
        LogApp::error("Failed to wait for buffer map async: {}", magic_enum::enum_name(waitStatus));
        return;
    }

    if (!userData.durations.empty()) {
        std::stringstream ss;
        for (const auto& duration : userData.durations) {
            ss << duration << "\n";
        }
        const auto nowCount = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const std::string fileName = std::string("timestamps_") + std::to_string(nowCount) + ".txt";
        FileSystem::writeFile(fileName, ss.str().c_str(), ss.str().size());
        LogApp::info("Timestamps saved to file: {}", fileName);

        FileSystem::download(fileName, fileName);
    } else {
        LogApp::warning("No timestamps to save");
    }
}
