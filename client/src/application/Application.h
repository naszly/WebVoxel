#pragma once

#include <vector>
#include <memory>

#include "graphics/Camera.h"
#include "Layer.h"
#include "core/Window.h"
#include "graphics/BlockTextureManager.h"
#include "graphics/TextureArray.h"
#include "world/World.h"

struct ApplicationData {
    VoxelData selectedVoxel = VoxelData(1);
    int placedVoxelRadius = 0;
    bool placedVoxelShapeIsSphere = true;

    size_t renderedChunks = 0;
    size_t renderedVoxels = 0;
};

class Application {
public:
    static Application& getInstance();

    void start(int width, int height);

    void stop();

    void pushLayer(std::shared_ptr<Layer> layer) {
        m_layers.push_back(std::move(layer));
    }

    [[nodiscard]] std::shared_ptr<WebGpuContext> getWebGpuContext() const {
        return m_window->getWebGpuContext();
    }

    [[nodiscard]] const WebGpuSurface& getWebGpuSurface() const {
        return m_window->getWebGpuSurface();
    }

    [[nodiscard]] const Input& getInput() const {
        return m_window->getInput();
    }

    [[nodiscard]] GLFWwindow* getGlfwWindow() const {
        return m_window->getGlfwWindow();
    }

    [[nodiscard]] World& getWorld() {
        return m_world;
    }

    Camera& getCamera() {
        return m_camera;
    }

    std::shared_ptr<BlockTextureManager> getBlockTextureManager() const {
        return m_blockTextureManager;
    }

    ApplicationData& getApplicationData() {
        return m_applicationData;
    }

    template<typename T>
    std::shared_ptr<T> getSystem() {
        for (const auto& layer : m_layers) {
            if (auto system = layer->getSystem<T>()) {
                return system;
            }
        }
        return nullptr;
    }

private:
    Application() = default;

    std::vector<std::shared_ptr<Layer>> m_layers;
    std::unique_ptr<Window> m_window;
    World m_world;
    Camera m_camera;
    std::shared_ptr<BlockTextureManager> m_blockTextureManager;

    ApplicationData m_applicationData;

    void onEvent(Event &event);

    void update(float deltaTime);

    WGPUTextureView getNextSurfaceTextureView() const;

    void render();

    void mainLoop();

#if defined(__EMSCRIPTEN__)
    static void emscriptenMainLoop(void* arg);
#endif
};