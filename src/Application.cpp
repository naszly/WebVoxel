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

void Application::start() {
    m_Window = Window::create({
            800,
            600,
            "Voxel WebGPU",
            [this](Event &event) { onEvent(event); }
        });

    for (const auto& layer : m_Layers) {
        layer->initialize();
    }

    m_World.generate();

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(Application::emscriptenMainLoop, this, 0, true);
#else
    while (!m_Window->shouldClose()) {
        mainLoop();
    }
#endif
}

void Application::onEvent(Event &event) {
    EventDispatcher dispatcher(event);

    /*dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent &event) {
        m_Window->close();
        return true;
    });*/

    for (const auto & m_Layer : std::ranges::reverse_view(m_Layers)) {
        m_Layer->onEvent(event);
        if (event.handled) {
            break;
        }
    }
}

void Application::mainLoop() {
    const auto now = std::chrono::high_resolution_clock::now();
    static auto lastTime = now;

    const float deltaTime = std::chrono::duration<float>(now - lastTime).count();

    m_Window->pollEvents();

    for (const auto& layer : m_Layers) {
        layer->update(deltaTime);
    }

    for (const auto& layer : m_Layers) {
        layer->render();
    }

    lastTime = now;
}
