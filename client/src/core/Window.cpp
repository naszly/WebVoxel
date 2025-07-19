#include "Window.h"

#include "events/KeyEvent.h"
#include "events/MouseEvent.h"
#include "events/ApplicationEvent.h"
#include "common/Log.h"

#include <GLFW/glfw3.h>


Window::Window(const WindowCreationConfig& config) : m_eventCallback(config.eventCallback) {

    glfwSetErrorCallback([](const int error, const char* description) {
        LogCore::error("GLFW Error ({0}): {1}", error, description);
    });

    if (!glfwInit()) {
        LogCore::critical("Failed to initialize GLFW");
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_Window = glfwCreateWindow(config.width, config.height, config.title, nullptr, nullptr);

    if (!m_Window) {
        glfwTerminate();
        LogCore::critical("Failed to create GLFW window");
        return;
    }

    m_Input = std::make_unique<Input>(m_Window);

    glfwSetWindowUserPointer(m_Window, this);

    m_WebGPUContext = std::make_shared<WebGPUContext>();
    m_WebGPUSurface = std::make_unique<WebGPUSurface>(m_Window, m_WebGPUContext);

    glfwSetKeyCallback(m_Window, [](GLFWwindow *glfwWindow, const int key, const int scancode, const int action, const int mods) {
        const Window* window = static_cast<Window *>(glfwGetWindowUserPointer(glfwWindow));

        switch (action) {
            case GLFW_PRESS: {
                KeyPressedEvent event(static_cast<const KeyCode>(key), false);
                window->m_eventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                KeyReleasedEvent event(static_cast<const KeyCode>(key));
                window->m_eventCallback(event);
                break;
            }
            case GLFW_REPEAT: {
                KeyPressedEvent event(static_cast<const KeyCode>(key), true);
                window->m_eventCallback(event);
                break;
            }
            default: {
                LogCore::warning("Unknown key action: {0}", action);
                break;
            }
        }
    });

    glfwSetMouseButtonCallback(m_Window, [](GLFWwindow *glfwWindow, const int button, const int action, const int mods) {
        const Window* window = static_cast<Window *>(glfwGetWindowUserPointer(glfwWindow));

        switch (action) {
            case GLFW_PRESS: {
                MouseButtonPressedEvent event(static_cast<const MouseCode>(button));
                window->m_eventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                MouseButtonReleasedEvent event(static_cast<const MouseCode>(button));
                window->m_eventCallback(event);
                break;
            }
            default: {
                LogCore::warning("Unknown mouse button action: {0}", action);
                break;
            }
        }
    });

    glfwSetScrollCallback(m_Window, [](GLFWwindow *glfwWindow, const double xOffset, const double yOffset) {
        const Window* window = static_cast<Window *>(glfwGetWindowUserPointer(glfwWindow));

        MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
        window->m_eventCallback(event);
    });

    glfwSetCursorPosCallback(m_Window, [](GLFWwindow *glfwWindow, const double xPos, const double yPos) {
        const Window* window = static_cast<Window *>(glfwGetWindowUserPointer(glfwWindow));

        MouseMovedEvent event(static_cast<float>(xPos), static_cast<float>(yPos));
        window->m_eventCallback(event);
    });

    glfwSetWindowSizeCallback(m_Window, [](GLFWwindow *glfwWindow, const int width, const int height) {
        Window* window = static_cast<Window *>(glfwGetWindowUserPointer(glfwWindow));

        window->m_WebGPUSurface->resize();

        WindowResizedEvent event(width, height);
        window->m_eventCallback(event);
    });
}

Window::~Window() {
    m_WebGPUSurface = nullptr;
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

bool Window::shouldClose() {
    return glfwWindowShouldClose(m_Window) == GLFW_TRUE;
}

void Window::close() {
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

void Window::pollEvents() {
    glfwPollEvents();
}
