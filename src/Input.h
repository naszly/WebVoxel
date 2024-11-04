#pragma once

#include <utility>

#include "KeyCode.h"
#include "MouseCode.h"

struct GLFWwindow;

class Input {
public:
    explicit Input(GLFWwindow *window) : glfwWindow(window) {};

    ~Input() = default;

    Input(Input const &) = delete;

    void operator=(Input const &) = delete;

    [[nodiscard]] bool isKeyPressed(KeyCode keyCode) const;

    [[nodiscard]] bool isMouseButtonPressed(MouseCode mouseCode) const;

    [[nodiscard]] bool isMouseLeftButtonPressed() const;

    [[nodiscard]] bool isMouseMiddleButtonPressed() const;

    [[nodiscard]] bool isMouseRightButtonPressed() const;

    [[nodiscard]] std::pair<float, float> getCursorPosition() const;

private:
    GLFWwindow *glfwWindow;
};