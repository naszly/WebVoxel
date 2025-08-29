#include "Application.h"

#include "../common/Timer.h"
#include "../common/Log.h"

#include <chrono>
#include <ranges>

#if defined(__EMSCRIPTEN__)
void Application::emscriptenMainLoop(void* arg) {
    auto app = static_cast<Application*>(arg);
    app->mainLoop();
}
#endif

void Application::start(const int width, const int height) {
    m_window = Window::create({
            .width=width,
            .height=height,
            .title="Voxel WebGPU",
            .eventCallback=[this](Event &event) { onEvent(event); }
        });

    m_world = std::make_unique<World>();

    m_blockTextureManager = BlockTextureManagerBuilder(
        std::vector{
            "textures/grass_top.png",
            "textures/grass_side.png",
            "textures/dirt.png",
            "textures/stone.png",
            "textures/duskstone.png",
            "textures/blackrock.png",
            "textures/eclipse_crystal.png",
            "textures/moonlit_lantern.png",
        }, *getWebGpuContext())
    .setTextureForTopFace(BlockId::Grass, "textures/grass_top.png")
    .setTextureForSideFaces(BlockId::Grass, "textures/grass_side.png")
    .setTextureForBottomFace(BlockId::Grass, "textures/dirt.png")
    .setTextureForAllFaces(BlockId::Dirt, "textures/dirt.png")
    .setTextureForAllFaces(BlockId::Stone, "textures/stone.png")
    .setTextureForAllFaces(BlockId::Duskstone, "textures/duskstone.png")
    .setTextureForAllFaces(BlockId::Blackrock, "textures/blackrock.png")
    .setTextureForAllFaces(BlockId::EclipseCrystal, "textures/eclipse_crystal.png")
    .setTextureForAllFaces(BlockId::MoonlitLantern, "textures/moonlit_lantern.png")
    .build();

    for (const auto& layer : m_layers) {
        layer->initialize();
    }

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(Application::emscriptenMainLoop, this, 0, true);
#else
    while (!m_window->shouldClose()) {
        mainLoop();
    }
#endif

    cleanUp();

    Timer::exportTimes();
}

void Application::stop() {
#if defined(__EMSCRIPTEN__)
    emscripten_cancel_main_loop();
#else
    m_window->close();
#endif
}

void Application::cleanUp() {
    for (auto& layer : m_layers) {
        layer.reset();
    }
    m_layers.clear();

    m_window.reset();
    m_world.reset();
    m_blockTextureManager.reset();
}

void Application::onEvent(Event &event) {
    for (const auto & layer : std::ranges::reverse_view(m_layers)) {
        layer->onEvent(event);
        if (event.handled) {
            break;
        }
    }
}

void Application::update(const float deltaTime) {
    m_window->pollEvents();

    for (const auto& layer : m_layers) {
        layer->update(deltaTime);
    }
}

WGPUTextureView Application::getNextSurfaceTextureView() const {
    const auto surface = getWebGpuSurface().getSurface();
    // Get the surface texture
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
        LogApp::error("Failed to get current surface texture: status {0}", static_cast<int>(surfaceTexture.status));
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
        LogApp::error("Failed to create texture view for surface texture");
    }

    return targetView;
}

void Application::render() {
    auto device = getWebGpuContext()->getDevice();
    const auto surface = getWebGpuSurface().getSurface();
    const auto queue = wgpuDeviceGetQueue(device);

    // Create a command encoder for the draw call
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = WGPUStringView{"My command encoder", WGPU_STRLEN};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    // Get the next target texture view
    WGPUTextureView targetView = getNextSurfaceTextureView();
    if (!targetView) return;

    for (const auto& layer : m_layers) {
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

ApplicationBuilder::ApplicationBuilder() = default;

size_t ApplicationBuilder::addLayer() {
    m_layers.push_back(std::make_unique<Layer>());
    return m_layers.size() - 1;
}

Application ApplicationBuilder::build() {
    Application app(std::move(m_layers));
    for (const auto& layer : app.m_layers) {
        layer->setAppReference(app);
    }
    return app;
}
