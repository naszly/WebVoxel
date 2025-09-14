#pragma once

#include <vector>
#include <memory>

#include "graphics/Camera.h"
#include "Layer.h"
#include "common/Exception.h"
#include "core/Window.h"
#include "graphics/BlockTextureManager.h"
#include "world/World.h"

struct ApplicationData {
    VoxelData selectedVoxel = VoxelData(BlockId::Grass);
    int placedVoxelRadius = 0;
    bool placedVoxelShapeIsSphere = true;

    size_t renderedChunks = 0;
    size_t renderedVoxels = 0;
};

class Application {
    friend class ApplicationBuilder;
public:
    void start(int width, int height);

    void stop();

    void cleanUp();

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

    [[nodiscard]] World& getWorld() const {
        return *m_world;
    }

    Camera& getCamera() {
        return m_camera;
    }

    [[nodiscard]] BlockTextureManager& getBlockTextureManager() const {
        return *m_blockTextureManager;
    }

    ApplicationData& getApplicationData() {
        return m_applicationData;
    }

    template<typename T>
    T* getSystem() {
        for (const auto& layer : m_layers) {
            if (auto system = layer->getSystem<T>()) {
                return system;
            }
        }
        return nullptr;
    }

private:
    explicit Application(std::vector<std::unique_ptr<Layer>> layers) : m_layers(std::move(layers)) {}

    std::vector<std::unique_ptr<Layer>> m_layers;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<World> m_world;
    Camera m_camera;
    std::unique_ptr<BlockTextureManager> m_blockTextureManager;

    ApplicationData m_applicationData;

    void onEvent(Event &event);

    void update(float deltaTime);

    [[nodiscard]] WGPUTextureView getNextSurfaceTextureView() const;

    void render();

    void mainLoop();

#if defined(__EMSCRIPTEN__)
    static void emscriptenMainLoop(void* arg);
#endif
};

class ApplicationBuilder {
public:
    ApplicationBuilder();
    ~ApplicationBuilder() = default;
    ApplicationBuilder(const ApplicationBuilder&) = delete;
    ApplicationBuilder(ApplicationBuilder&&) = delete;
    ApplicationBuilder& operator=(const ApplicationBuilder&) = delete;
    ApplicationBuilder& operator=(ApplicationBuilder&&) = delete;

    // Add a new layer and return its index
    size_t addLayer();

    // Add a system to a specific layer
    template<typename T, typename... Args>
    ApplicationBuilder& addSystemToLayer(const size_t layerIndex, Args&&... args) {
        static_assert(std::is_base_of_v<System, T>, "T must inherit from System");
        if (layerIndex >= m_layers.size()) throw Exception("Layer index out of bounds");
        m_layers[layerIndex]->pushSystem<T>(std::forward<Args>(args)...);
        return *this;
    }

    Application build();
private:
    std::vector<std::unique_ptr<Layer>> m_layers;
};
