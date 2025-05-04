#pragma once

#include <memory>

#include "WebGPUContext.h"
#include "WebGPUSurface.h"
#include "Input.h"
#include "Event.h"

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

    [[nodiscard]] std::shared_ptr<WebGPUContext> getWebGPUContext() const {
        return m_WebGPUContext;
    }

    [[nodiscard]] const WebGPUSurface& getWebGPUSurface() const {
        return *m_WebGPUSurface;
    }

    [[nodiscard]] const Input& getInput() const {
        return *m_Input;
    }

    [[nodiscard]] GLFWwindow* getGLFWWindow() const {
        return m_Window;
    }

private:
    GLFWwindow* m_Window;
    std::shared_ptr<WebGPUContext> m_WebGPUContext;
    std::unique_ptr<WebGPUSurface> m_WebGPUSurface;
    std::unique_ptr<Input> m_Input;
    EventCallbackFn eventCallback;
};
