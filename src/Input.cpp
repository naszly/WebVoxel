//
// Created by kornel on 14/10/24.
//

#include "Input.h"

#include <GLFW/glfw3.h>


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
