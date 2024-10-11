#include <iostream>

#include "Log.h"
#include "Application.h"
#include "systems/ControllerSystem.h"
#include "systems/RendererSystem.h"

int main() {
    LogCore::info("Hello, World!");

    printf("Hello, World!\n");
    std::cout << "Hello, World!" << std::endl;

    auto layer = std::make_shared<Layer>();

    auto rendererSystem = std::make_shared<RendererSystem>();
    auto controllerSystem = std::make_shared<ControllerSystem>();

    layer->pushSystem(rendererSystem);
    layer->pushSystem(controllerSystem);

    Application& app = Application::GetInstance();
    app.pushLayer(layer);
    app.start();

    return 0;
}
