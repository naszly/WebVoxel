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

    const int width = (argc > 1) ? std::stoi(argv[1]) : 800;
    const int height = (argc > 2) ? std::stoi(argv[2]) : 600;
    const std::optional<int> voxWorldModelIndex = (argc > 3) ? std::make_optional(std::stoi(argv[3])) : std::nullopt;

    // Build application layers and systems
    ApplicationBuilder builder;

    const size_t mainLayerIdx = builder.addLayer();
    builder.addSystemToLayer<RendererSystem>(mainLayerIdx)
           .addSystemToLayer<ControllerSystem>(mainLayerIdx);
    if (voxWorldModelIndex) {
        builder.addSystemToLayer<VoxWorldLoaderSystem>(mainLayerIdx, *voxWorldModelIndex);
    } else {
        builder.addSystemToLayer<ChunkManagementSystem>(mainLayerIdx);
    }

    const size_t guiLayerIdx = builder.addLayer();
    builder.addSystemToLayer<GuiSystem>(guiLayerIdx);

    Application app = builder.build();

    try {
        app.start(width, height);
    } catch (const std::exception& e) {
        app.cleanUp();
        LogCore::critical("Application encountered an error: {}", e.what());
        return 1;
    }

    return 0;
}
