#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include <cxxopts.hpp>

#include "common/Log.h"
#include "application/Application.h"
#include "common/FileSystem.h"
#include "application/systems/ChunkManagementSystem.h"
#include "application/systems/ChunkVertexDataUpdaterSystem.h"
#include "application/systems/ControllerSystem.h"
#include "application/systems/GuiSystem.h"
#include "application/systems/InventorySystem.h"
#include "application/systems/RendererSystem.h"
#include "application/systems/VoxWorldLoaderSystem.h"

int main(const int argc, char** argv) {
    LogCore::info("Hello, World!");
    FileSystem::initialize();

    int width = 800;
    int height = 600;
    bool cavesEnabled = true;
    int seed = 0;
    std::string worldName;
    std::string roomCode;
    std::string roomToken;
    bool createWorld = false;
    bool listWorlds = false;
    std::optional<int> voxWorldModelIndex = std::nullopt;

    try {
        cxxopts::Options options("WebVoxel", "Voxel Renderer CLI");

        options.add_options()
            ("width", "Window width", cxxopts::value<int>()->default_value("800"))
            ("height", "Window height", cxxopts::value<int>()->default_value("600"))
            ("modelIndex", "Vox model index", cxxopts::value<int>())
            ("caves", "Enable cave generation", cxxopts::value<bool>()->default_value("true"))
            ("seed", "Seed for world generation", cxxopts::value<int>()->default_value("0"))
            ("world", "World name", cxxopts::value<std::string>())
            ("room", "Multiplayer room code", cxxopts::value<std::string>())
            ("room-token", "Multiplayer room access token", cxxopts::value<std::string>())
            ("create", "Create the world if it doesn't exist")
            ("list-worlds", "List available worlds")
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
        cavesEnabled = result["caves"].as<bool>();
        seed = result["seed"].as<int>();

        if (result.count("world")) {
            worldName = result["world"].as<std::string>();

            const bool valid = !worldName.empty() &&
                std::ranges::all_of(worldName, [](const unsigned char c) {
                    return std::islower(c) || std::isdigit(c) || c == '_';
                });

            if (!valid) {
                throw std::runtime_error(
                    "Invalid world name. World names may only contain lowercase letters (a-z), digits (0-9), and underscores (_).");
            }
        }

        if (result.count("room")) {
            roomCode = result["room"].as<std::string>();
            const std::string_view alphabet = "ABCDEFGHJKMNPQRTUVWXYZ2346789";
            const bool valid = roomCode.size() == 6 &&
                std::ranges::all_of(roomCode, [&alphabet](const char character) {
                    return alphabet.contains(character);
                });
            if (!valid) {
                throw std::runtime_error("Invalid multiplayer room code.");
            }
        }

        if (result.count("room-token")) {
            roomToken = result["room-token"].as<std::string>();
            const bool valid = roomToken.size() == 32 &&
                std::ranges::all_of(roomToken, [](const unsigned char character) {
                    return std::isdigit(character) || (character >= 'A' && character <= 'F');
                });
            if (!valid) {
                throw std::runtime_error("Invalid multiplayer room access token.");
            }
        }

        if (roomCode.empty() != roomToken.empty()) {
            throw std::runtime_error("A multiplayer room code and access token must be provided together.");
        }

        createWorld = result.count("create") > 0;

        listWorlds = result.count("list-worlds") > 0;


        if (listWorlds)
        {
            for (const auto worlds = FileSystem::listDirectories("./worlds/"); const auto& world : worlds)
            {
                std::cout << world << "\n";
            }

            return 0;
        }

    } catch (const std::exception& e) {
        LogCore::critical("Failed to parse CLI arguments: {}", e.what());
        return 1;
    }

    // Build application layers and systems
    ApplicationBuilder builder;

    const size_t mainLayerIdx = builder.addLayer();
    builder.addSystemToLayer<RendererSystem>(mainLayerIdx)
           .addSystemToLayer<ChunkVertexDataUpdaterSystem>(mainLayerIdx)
           .addSystemToLayer<ControllerSystem>(mainLayerIdx);
    if (voxWorldModelIndex) {
        builder.addSystemToLayer<VoxWorldLoaderSystem>(mainLayerIdx, *voxWorldModelIndex);
    } else if (!worldName.empty()) {
        if (createWorld) {
            WorldGeneratorParams params{
                .seed = seed,
                .cavesEnabled = cavesEnabled,
            };

            builder.addSystemToLayer<ChunkManagementSystem>(
                mainLayerIdx,
                "worlds/" + worldName,
                params,
                roomCode,
                roomToken);
        } else {
            builder.addSystemToLayer<ChunkManagementSystem>(
                mainLayerIdx,
                "worlds/" + worldName,
                std::nullopt,
                roomCode,
                roomToken);
        }
    }

    const size_t guiLayerIdx = builder.addLayer();
    builder.addSystemToLayer<GuiSystem>(guiLayerIdx)
           .addSystemToLayer<InventorySystem>(guiLayerIdx);

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
