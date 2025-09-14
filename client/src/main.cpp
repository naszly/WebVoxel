#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include <cxxopts.hpp>

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

    int width = 800;
    int height = 600;
    std::optional<int> voxWorldModelIndex = std::nullopt;

    try {
        cxxopts::Options options("WebVoxel", "Voxel Renderer CLI");

        options.add_options()
            ("width", "Window width", cxxopts::value<int>()->default_value("800"))
            ("height", "Window height", cxxopts::value<int>()->default_value("600"))
            ("modelIndex", "Vox model index", cxxopts::value<int>())
            ("help", "Print help");

        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return 0;
        }

        width = result["width"].as<int>();
        height = result["height"].as<int>();
        if (result.count("modelIndex")) {
            voxWorldModelIndex = result["modelIndex"].as<int>();
        }

        LogCore::info("Parsed arguments: width={}, height={}, modelIndex={}", width, height,
                      voxWorldModelIndex ? std::to_string(*voxWorldModelIndex) : "none");

    } catch (const std::exception& e) {
        LogCore::critical("Failed to parse CLI arguments: {}", e.what());
        return 1;
    }

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
