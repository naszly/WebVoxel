#include "Application.h"

#include <chrono>
#include <ranges>

Application & Application::GetInstance() {
    static Application instance;
    return instance;
}

#if defined(__EMSCRIPTEN__)
void Application::emscriptenMainLoop(void* arg) {
    auto app = static_cast<Application*>(arg);
    app->mainLoop();
}
#endif

void Application::start(const int width, const int height) {
    m_Window = Window::create({
            .width=width,
            .height=height,
            .title="Voxel WebGPU",
            .eventCallback=[this](Event &event) { onEvent(event); }
        });

    for (const auto& layer : m_Layers) {
        layer->initialize();
    }

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(Application::emscriptenMainLoop, this, 0, true);
#else
    while (!m_Window->shouldClose()) {
        mainLoop();
    }
#endif

    Timer::exportTimes();
}

void Application::stop() {
#if defined(__EMSCRIPTEN__)
    emscripten_cancel_main_loop();
#else
    m_Window->close();
#endif
}

void Application::onEvent(Event &event) {
    for (const auto & layer : std::ranges::reverse_view(m_Layers)) {
        layer->onEvent(event);
        if (event.handled) {
            break;
        }
    }
}

void Application::update(const float deltaTime) {
    m_Window->pollEvents();

    for (const auto& layer : m_Layers) {
        layer->update(deltaTime);
    }
}

WGPUTextureView Application::getNextSurfaceTextureView() const {
    const auto surface = getWebGPUSurface().getSurface();
    // Get the surface texture
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
        LogCore::error("Failed to get current surface texture: status {0}", static_cast<int>(surfaceTexture.status));
        return nullptr;
    }

    // Create a view for this surface texture
    WGPUTextureViewDescriptor viewDescriptor = {};
    viewDescriptor.nextInChain = nullptr;
    viewDescriptor.label = WGPUStringView{"Surface texture view", WGPU_STRLEN};
    viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
    viewDescriptor.dimension = WGPUTextureViewDimension_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = WGPUTextureAspect_All;
    WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);

    if (!targetView) {
        LogCore::error("Failed to create texture view for surface texture");
    }

    return targetView;
}

void Application::render() {
    auto device = getWebGPUContext()->getDevice();
    const auto surface = getWebGPUSurface().getSurface();
    const auto queue = wgpuDeviceGetQueue(device);

    // Create a command encoder for the draw call
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = WGPUStringView{"My command encoder", WGPU_STRLEN};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    // Get the next target texture view
    WGPUTextureView targetView = getNextSurfaceTextureView();
    if (!targetView) return;

    for (const auto& layer : m_Layers) {
        layer->render(encoder, targetView);
    }

    // Encode and submit the render pass
    WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.nextInChain = nullptr;
    cmdBufferDescriptor.label = WGPUStringView{"Command buffer", WGPU_STRLEN};
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &command);
    wgpuCommandBufferRelease(command);

    // At the end of the frame
    wgpuTextureViewRelease(targetView);
#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(surface);
#endif

#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(device, false, nullptr);
#endif
}

void Application::mainLoop() {
    const auto now = std::chrono::high_resolution_clock::now();
    static auto lastTime = now;

    const float deltaTime = std::chrono::duration<float>(now - lastTime).count();

    update(deltaTime);

    render();

    lastTime = now;
}
