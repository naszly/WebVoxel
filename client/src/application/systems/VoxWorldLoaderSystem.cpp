#include "VoxWorldLoaderSystem.h"

#include "common/FileSystem.h"
#include "common/Log.h"

#define OGT_VOX_IMPLEMENTATION
#include "ogt_vox.h"

void VoxWorldLoaderSystem::initialize() {

    const std::string models[] {
        "models/sponza15.vox", // ~1 million
        "models/sponza22.vox", // ~2 million
        "models/sponza33.vox", // ~4 million
        "models/sponza46.vox", // ~8 million
    };

    constexpr glm::ivec3 offsets[] {
        {0,15,0},
        {0,22,0},
        {0,33,0},
        {0,46,0},
    };

    const std::vector<char> buffer = FileSystem::readFileNative(models[m_modelIndex]);
    const ogt_vox_scene* scene = ogt_vox_read_scene(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());

    if (!scene) {
        LogApp::error("Failed to load VOX scene.");
        return;
    }

    LogApp::info("scene->num_models: {0}", scene->num_models);
    LogApp::info("scene->num_instances: {0}", scene->num_instances);
    LogApp::info("scene->num_layers: {0}", scene->num_layers);
    LogApp::info("scene->num_groups: {0}", scene->num_groups);
    LogApp::info("scene->num_cameras: {0}", scene->num_cameras);

    World& world = getWorld();

    for (uint32_t i = 0; i < scene->num_instances; ++i) {
        const ogt_vox_instance& instance = scene->instances[i];
        const ogt_vox_model* model = scene->models[instance.model_index];

        if (!model) continue;

        const auto magicOffset = glm::ivec3(instance.transform.m30 - model->size_x / 2.0,
                                            instance.transform.m32 - model->size_z / 2.0,
                                            instance.transform.m31 - model->size_y / 2.0) - offsets[m_modelIndex];

        const auto size = glm::ivec3(model->size_x, model->size_z, model->size_y);

        const auto start = WorldCoordinate(magicOffset).chunkPosition() + glm::ivec3(-1, -1, -1);
        const auto end = WorldCoordinate(magicOffset + size).chunkPosition() + glm::ivec3(1, 1, 1);

        for (int x = start.x; x <= end.x; ++x) {
            for (int y = start.y; y <= end.y; ++y) {
                for (int z = start.z; z <= end.z; ++z) {
                     if (!world.hasChunk({x, y, z}))
                         world.createChunk({x, y, z});
                }
            }
        }

        uint32_t voxelIndex = 0;
        for (uint32_t z = 0; z < model->size_z; z++) {
            for (uint32_t y = 0; y < model->size_y; y++) {
                for (uint32_t x = 0; x < model->size_x; x++, voxelIndex++) {
                    const uint8_t colorIndex = model->voxel_data[voxelIndex];

                    if (colorIndex == 0) continue; // Skip empty voxels.

                    const auto&[r, g, b, a] = scene->palette.color[colorIndex];
                    const VoxelData voxel(/*r, g, b, a*/ colorIndex); // TODO: handle palette colors properly

                    glm::ivec3 worldPosition = glm::ivec3(x, z, y) + magicOffset;
                    world.setVoxel(WorldCoordinate(worldPosition), voxel);
                }
            }
        }
    }

    ogt_vox_destroy_scene(scene);
}

void VoxWorldLoaderSystem::render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) {
}

void VoxWorldLoaderSystem::update(float dt) {
}

void VoxWorldLoaderSystem::onEvent(Event &event) {
}
