#include "System.h"
#include "../Application.h"

Camera& System::GetCamera() {
    return Application::GetInstance().getCamera();
}

const Input& System::GetInput() {
    return Application::GetInstance().getInput();
}

World& System::GetWorld() {
    return Application::GetInstance().getWorld();
}

const WebGPUContext& System::GetWebGPUContext() {
    return *Application::GetInstance().getWebGPUContext();
}

const WebGPUSurface& System::GetWebGPUSurface() {
    return Application::GetInstance().getWebGPUSurface();
}
