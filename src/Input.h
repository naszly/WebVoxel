#pragma once

#include <utility>
#include <GLFW/glfw3.h>

#include "KeyCode.h"
#include "MouseCode.h"

enum class CursorMode {
    Normal = GLFW_CURSOR_NORMAL,
    Hidden = GLFW_CURSOR_HIDDEN,
    Disabled = GLFW_CURSOR_DISABLED,
    //Captured = GLFW_CURSOR_CAPTURED
};
/*
enum class CursorShape {
    Arrow = GLFW_ARROW_CURSOR,
    IBeam = GLFW_IBEAM_CURSOR,
    Crosshair = GLFW_CROSSHAIR_CURSOR,
    Hand = GLFW_POINTING_HAND_CURSOR,
    ResizeEastWest = GLFW_RESIZE_EW_CURSOR,
    ResizeNorthSouth = GLFW_RESIZE_NS_CURSOR,
    ResizeNorthEastSouthWest = GLFW_RESIZE_NESW_CURSOR,
    ResizeNorthWestSouthEast = GLFW_RESIZE_NWSE_CURSOR,
    ResizeAll = GLFW_RESIZE_ALL_CURSOR,
    NotAllowed = GLFW_NOT_ALLOWED_CURSOR
};*/

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

    void setCursorMode(CursorMode mode) const;

    //void setCursorShape(CursorShape shape);

private:
    GLFWwindow *glfwWindow;
    GLFWcursor *glfwCursor{nullptr};
    //CursorShape cursorShape{CursorShape::Arrow};
};