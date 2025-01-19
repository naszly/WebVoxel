#pragma once

#include <vector>
#include <memory>

#include "layers/Layer.h"
#include "Window.h"
#include "world/World.h"
#include "Camera.h"

struct ApplicationData {
    glm::vec4 placedVoxelColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    int placedVoxelRadius = 0;
    bool placedVoxelShapeIsSphere = true;

    size_t renderedChunks = 0;
    size_t renderedVoxels = 0;
};

class Application {
public:
    static Application& GetInstance();

    void start();

    void pushLayer(std::shared_ptr<Layer> layer) {
        m_Layers.push_back(std::move(layer));
    }

    [[nodiscard]] std::shared_ptr<WebGPUContext> getWebGPUContext() const {
        return m_Window->getWebGPUContext();
    }

    [[nodiscard]] const WebGPUSurface& getWebGPUSurface() const {
        return m_Window->getWebGPUSurface();
    }

    [[nodiscard]] const Input& getInput() const {
        return m_Window->getInput();
    }

    [[nodiscard]] GLFWwindow* getGLFWWindow() const {
        return m_Window->getGLFWWindow();
    }

    [[nodiscard]] World& getWorld() {
        return m_World;
    }

    Camera& getCamera() {
        return m_Camera;
    }

    ApplicationData& getApplicationData() {
        return m_ApplicationData;
    }

private:
    Application() = default;

    std::vector<std::shared_ptr<Layer>> m_Layers;
    std::unique_ptr<Window> m_Window;
    World m_World;
    Camera m_Camera;

    ApplicationData m_ApplicationData;

    void onEvent(Event &event);

    void update(float deltaTime);

    WGPUTextureView getNextSurfaceTextureView() const;

    void render();

    void mainLoop();

#if defined(__EMSCRIPTEN__)
    static void emscriptenMainLoop(void* arg);
#endif
};