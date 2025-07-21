#pragma once

#include <memory>

#include "webgpu/WebGPUContext.h"
#include "webgpu/WebGPUSurface.h"
#include "Input.h"
#include "events/Event.h"

struct GLFWwindow;

struct WindowCreationConfig {
    int width;
    int height;
    const char* title;
    EventCallbackFn eventCallback;
};

class Window {
public:
    explicit Window(const WindowCreationConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) = delete;

    static std::unique_ptr<Window> create(const WindowCreationConfig& config) {
        return std::make_unique<Window>(config);
    }

    bool shouldClose();

    void close();

    void pollEvents();

    [[nodiscard]] std::shared_ptr<WebGpuContext> getWebGpuContext() const {
        return m_webGpuContext;
    }

    [[nodiscard]] const WebGpuSurface& getWebGpuSurface() const {
        return *m_webGpuSurface;
    }

    [[nodiscard]] const Input& getInput() const {
        return *m_input;
    }

    [[nodiscard]] GLFWwindow* getGlfwWindow() const {
        return m_window;
    }

private:
    GLFWwindow* m_window;
    std::shared_ptr<WebGpuContext> m_webGpuContext;
    std::unique_ptr<WebGpuSurface> m_webGpuSurface;
    std::unique_ptr<Input> m_input;
    EventCallbackFn m_eventCallback;
};
