#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "common/Log.h"
#include "Application.h"
#include "common/FileSystem.h"
#include "systems/ChunkManagementSystem.h"
#include "systems/ControllerSystem.h"
#include "systems/GuiSystem.h"
#include "systems/RendererSystem.h"
#include "systems/VoxWorldLoaderSystem.h"

int main(const int argc, char** argv) {

    LogCore::info("Hello, World!");

    FileSystem::initialize();

    Application& app = Application::getInstance();

    const auto layer = std::make_shared<Layer>();

    const int width = (argc > 1) ? std::stoi(argv[1]) : 800;
    const int height = (argc > 2) ? std::stoi(argv[2]) : 600;
    const std::optional<int> voxWorldModelIndex = (argc > 3) ? std::make_optional(std::stoi(argv[3])) : std::nullopt;

    const auto rendererSystem = std::make_shared<RendererSystem>();
    const auto controllerSystem = std::make_shared<ControllerSystem>();
    const auto worldSystem = voxWorldModelIndex.has_value()
        ? std::static_pointer_cast<System>(std::make_shared<VoxWorldLoaderSystem>(voxWorldModelIndex.value()))
        : std::static_pointer_cast<System>(std::make_shared<ChunkManagementSystem>());

    layer->pushSystem(rendererSystem);
    layer->pushSystem(controllerSystem);
    layer->pushSystem(worldSystem);

    app.pushLayer(layer);

    const auto guiLayer = std::make_shared<Layer>();
    const auto guiSystem = std::make_shared<GuiSystem>();

    guiLayer->pushSystem(guiSystem);

    app.pushLayer(guiLayer);

    try {
        app.start(width, height);
    } catch (const std::exception& e) {
        LogCore::critical("Application encountered an error: {}", e.what());
        return 1;
    }

    return 0;
}
