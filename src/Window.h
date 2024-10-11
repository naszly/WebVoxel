#pragma once

#include <glfw3webgpu.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "WebGPUContext.h"
#include "Input.h"
#include "Event.h"
#include "KeyEvent.h"
#include "MouseEvent.h"

#include <memory>


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

    static std::unique_ptr<Window> create(WindowCreationConfig config) {
        return std::make_unique<Window>(config);
    }

    bool shouldClose() {
        return glfwWindowShouldClose(m_Window) == GLFW_TRUE;
    }

    void pollEvents() {
        glfwPollEvents();
    }

    std::shared_ptr<WebGPUContext> getWebGPUContext() const {
        return m_WebGPUContext;
    }

    const Input& getInput() const {
        return *m_Input;
    }

private:
    GLFWwindow* m_Window;
    std::shared_ptr<WebGPUContext> m_WebGPUContext;
    std::unique_ptr<Input> m_Input;
    EventCallbackFn eventCallback;

    void onEvent(Event &event);
};
