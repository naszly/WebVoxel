#pragma once

#include <vector>
#include <memory>

#include "layers/Layer.h"
#include "Window.h"
#include "world/World.h"
#include "Camera.h"

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
        return m_camera;
    }

private:
    Application() = default;

    std::vector<std::shared_ptr<Layer>> m_Layers;
    std::unique_ptr<Window> m_Window;
    World m_World;
    Camera m_camera;

    void onEvent(Event &event);

    void update(float deltaTime);

    WGPUTextureView getNextSurfaceTextureView() const;

    void render();

    void mainLoop();

#if defined(__EMSCRIPTEN__)
    static void emscriptenMainLoop(void* arg);
#endif
};