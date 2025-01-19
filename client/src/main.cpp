#include "Log.h"
#include "Application.h"
#include "systems/ChunkManagementSystem.h"
#include "systems/ControllerSystem.h"
#include "systems/GuiSystem.h"
#include "systems/RendererSystem.h"

int main() {
    LogCore::info("Hello, World!");

    Application& app = Application::GetInstance();

    const auto layer = std::make_shared<Layer>();

    const auto rendererSystem = std::make_shared<RendererSystem>();
    const auto controllerSystem = std::make_shared<ControllerSystem>();
    const auto chunkManagementSystem = std::make_shared<ChunkManagementSystem>();

    layer->pushSystem(rendererSystem);
    layer->pushSystem(controllerSystem);
    layer->pushSystem(chunkManagementSystem);

    app.pushLayer(layer);

    const auto guiLayer = std::make_shared<Layer>();
    const auto guiSystem = std::make_shared<GuiSystem>();

    guiLayer->pushSystem(guiSystem);

    app.pushLayer(guiLayer);

    app.start();

    return 0;
}
