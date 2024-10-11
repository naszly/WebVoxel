#include "System.h"
#include "../Application.h"

Camera& System::GetCamera() {
    return Application::GetInstance().getCamera();
}

const Input& System::GetInput() {
    return Application::GetInstance().getInput();
}

const World& System::GetWorld() {
    return Application::GetInstance().getWorld();
}