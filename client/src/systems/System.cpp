#include "core/System.h"
#include "Application.h"

Camera& System::getCamera() {
    return Application::getInstance().getCamera();
}

const Input& System::getInput() {
    return Application::getInstance().getInput();
}

World& System::getWorld() {
    return Application::getInstance().getWorld();
}

const WebGpuContext& System::getWebGpuContext() {
    return *Application::getInstance().getWebGpuContext();
}

const WebGpuSurface& System::getWebGpuSurface() {
    return Application::getInstance().getWebGpuSurface();
}

GLFWwindow * System::getGlfwWindow() {
    return Application::getInstance().getGlfwWindow();
}
