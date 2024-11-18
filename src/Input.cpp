//
// Created by kornel on 14/10/24.
//

#include "Input.h"

#include <GLFW/glfw3.h>

#include "imgui.h"

bool Input::isKeyPressed(KeyCode keyCode) const {
    auto state = glfwGetKey(glfwWindow, static_cast<int>(keyCode));
    return state == GLFW_PRESS;
}

bool Input::isMouseButtonPressed(MouseCode mouseCode) const {
    auto state = glfwGetMouseButton(glfwWindow, static_cast<int>(mouseCode));
    return state == GLFW_PRESS;
}

bool Input::isMouseLeftButtonPressed() const {
    return isMouseButtonPressed(MouseCode::ButtonLeft);
}

bool Input::isMouseMiddleButtonPressed() const {
    return isMouseButtonPressed(MouseCode::ButtonMiddle);
}

bool Input::isMouseRightButtonPressed() const {
    return isMouseButtonPressed(MouseCode::ButtonRight);
}

std::pair<float, float> Input::getCursorPosition() const {
    double x, y;
    glfwGetCursorPos(glfwWindow, &x, &y);
    return {x, y};
}

void Input::setCursorMode(const CursorMode mode) const {
    int value = 0;

    switch (mode) {
        case Normal:
            value = GLFW_CURSOR_NORMAL;
            break;
        case Hidden:
            value = GLFW_CURSOR_HIDDEN;
            break;
        case Disabled:
            value = GLFW_CURSOR_DISABLED;
            break;
    }

    glfwSetInputMode(glfwWindow, GLFW_CURSOR, value);

    ImGuiIO& io = ImGui::GetIO();
    if (mode == Disabled) {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }
}
