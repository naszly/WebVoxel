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

    Camera& getCamera() {
        return m_camera;
    }

    [[nodiscard]] const Input& getInput() const {
        return m_Window->getInput();
    }

    [[nodiscard]] const World& getWorld() const {
        return m_World;
    }

private:
    Application() = default;

    std::vector<std::shared_ptr<Layer>> m_Layers;
    std::unique_ptr<Window> m_Window;
    World m_World;
    Camera m_camera;

    void onEvent(Event &event);

    void mainLoop();

#if defined(__EMSCRIPTEN__)
    static void emscriptenMainLoop(void* arg);
#endif
};