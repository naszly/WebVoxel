#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "common/Log.h"
#include "application/Application.h"
#include "common/FileSystem.h"
#include "application/systems/ChunkManagementSystem.h"
#include "application/systems/ControllerSystem.h"
#include "application/systems/GuiSystem.h"
#include "application/systems/RendererSystem.h"
#include "application/systems/VoxWorldLoaderSystem.h"

int main(const int argc, char** argv) {

    LogCore::info("Hello, World!");

    FileSystem::initialize();

    Application& app = Application::getInstance();

    const int width = (argc > 1) ? std::stoi(argv[1]) : 800;
    const int height = (argc > 2) ? std::stoi(argv[2]) : 600;
    const std::optional<int> voxWorldModelIndex = (argc > 3) ? std::make_optional(std::stoi(argv[3])) : std::nullopt;

    auto& layer = app.createLayer();
    layer.pushSystem<RendererSystem>();
    layer.pushSystem<ControllerSystem>();
    voxWorldModelIndex.has_value()
        ? layer.pushSystem<VoxWorldLoaderSystem>(voxWorldModelIndex.value())
        : layer.pushSystem<ChunkManagementSystem>();

    auto& guiLayer = app.createLayer();
    guiLayer.pushSystem<GuiSystem>();

    try {
        app.start(width, height);
    } catch (const std::exception& e) {
        app.cleanUp();
        LogCore::critical("Application encountered an error: {}", e.what());
        return 1;
    }

    return 0;
}
