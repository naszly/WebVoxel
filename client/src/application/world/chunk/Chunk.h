#pragma once

#include <cassert>
#include <random>
#include <glm/vec3.hpp>
#include <vector>

#include "BitmappedVoxelTree.h"
#include "application/world/WorldGenerator.h"
#include "common/Utils.h"

class Chunk {
    static constexpr size_t TREE_DEPTH = 5;
    static constexpr size_t NODE_COUNT_PER_AXIS = 2;
    using SparseVoxelOctTree = BitmappedVoxelTree<TREE_DEPTH, NODE_COUNT_PER_AXIS>;
public:
    static constexpr size_t WIDTH = Utils::pow(NODE_COUNT_PER_AXIS, TREE_DEPTH);
    static_assert(WIDTH >= 16 && WIDTH <= 256, "SIZE must be between 16 and 256");

    explicit Chunk(const glm::ivec3 position)
        : m_position(position), m_lastEdit(std::chrono::steady_clock::now()) {}

    explicit Chunk(const int x, const int y, const int z)
        : m_position(x, y, z), m_lastEdit(std::chrono::steady_clock::now()) {}

    ~Chunk() = default;

    Chunk(const Chunk& other) {
        m_position = other.m_position;
        m_data = other.m_data;
        m_gpuBufferDirty = other.m_gpuBufferDirty;
        m_saveFileDirty = other.m_saveFileDirty;
        m_lastEdit = other.m_lastEdit;
        m_litVoxels = other.m_litVoxels;
    }

    Chunk& operator=(const Chunk& other) {
        if (this != &other) {
            m_position = other.m_position;
            m_data = other.m_data;
            m_gpuBufferDirty = other.m_gpuBufferDirty;
            m_saveFileDirty = other.m_saveFileDirty;
            m_lastEdit = other.m_lastEdit;
            m_litVoxels = other.m_litVoxels;
        }
        return *this;
    }

    Chunk(Chunk&& other) noexcept {
        m_position = other.m_position;
        m_data = std::move(other.m_data);
        m_gpuBufferDirty = other.m_gpuBufferDirty;
        m_saveFileDirty = other.m_saveFileDirty;
        m_lastEdit = other.m_lastEdit;
        other.m_gpuBufferDirty = false;
        other.m_saveFileDirty = false;
        m_litVoxels = std::move(other.m_litVoxels);
    }

    Chunk& operator=(Chunk&& other) noexcept {
        if (this != &other) {
            m_position = other.m_position;
            m_data = std::move(other.m_data);
            m_gpuBufferDirty = other.m_gpuBufferDirty;
            m_saveFileDirty = other.m_saveFileDirty;
            m_lastEdit = other.m_lastEdit;
            other.m_gpuBufferDirty = false;
            other.m_saveFileDirty = false;
            m_litVoxels = std::move(other.m_litVoxels);
        }
        return *this;
    }

    void generate(WorldGenerator& generator);

    [[nodiscard]] glm::ivec3 getPosition() const {
        return m_position;
    }

    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return m_data.getVoxel(x, y, z);
    }

    [[nodiscard]] bool hasVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return m_data.hasVoxel(x, y, z);
    }

    void setVoxel(const VoxelData& voxel, const uint32_t x, const uint32_t y, const uint32_t z) {
        setVoxelInternal(voxel, x, y, z);
        m_gpuBufferDirty = true;
        m_saveFileDirty = true;
        m_lastEdit = std::chrono::steady_clock::now();
    }

    void queueVoxelToSet(const VoxelData& voxel, const uint32_t x, const uint32_t y, const uint32_t z) {
        m_queuedVoxelsToSet.emplace_back(glm::ivec3(x, y, z), voxel);
    }

    void executeQueuedVoxelsToSet() {
        for (const auto& [pos, voxel] : m_queuedVoxelsToSet) {
            setVoxelInternal(voxel, pos.x, pos.y, pos.z);
        }
        m_queuedVoxelsToSet.resize(0);
        m_gpuBufferDirty = true;
        m_saveFileDirty = true;
        m_lastEdit = std::chrono::steady_clock::now();
    }

    bool isEmpty() const {
        return m_data.isEmpty();
    }

    [[nodiscard]] bool isGpuBufferDirty() const {
        return m_gpuBufferDirty;
    }

    void setGpuBufferDirty() {
        m_gpuBufferDirty = true;
    }

    void resetGpuBufferDirty() {
        m_gpuBufferDirty = false;
    }

    [[nodiscard]] bool isSaveFileDirty() const {
        return m_saveFileDirty;
    }

    void resetSaveFileDirty() {
        m_saveFileDirty = false;
    }

    [[nodiscard]] const auto& getBitmap() const {
        return m_data.getBitmap();
    }

    [[nodiscard]] bool fileExists(const std::string &path) const;

    void save(const std::string &path);

    void load(const std::string &path);

    void compress() {
        return m_data.compress();
    }

    bool isCompressed() const {
        return m_data.isCompressed();
    }

    std::chrono::steady_clock::time_point getLastEdit() const {
        return m_lastEdit;
    }

    static void cleanFs();

    struct QueuedVoxelOp {
        glm::ivec3 position{};
        VoxelData voxel;
    };

    std::vector<QueuedVoxelOp>& getQueuedVoxelsToSet() { return m_queuedVoxelsToSet; }

    struct LightSource {
        int x, y, z;
        BlockLightInfo lightInfo;
    };

    std::vector<LightSource> getLightSources(const int xOffset = 0, const int yOffset = 0, const int zOffset = 0) const {
        std::vector<LightSource> lights;
        lights.reserve(m_litVoxels.size());

        auto isSurrounded = [&](const int x, const int y, const int z) -> bool {
            static constexpr int DIRS[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            for (const auto &d : DIRS) {
                const uint32_t nx = x + d[0];
                const uint32_t ny = y + d[1];
                const uint32_t nz = z + d[2];
                if ((nx >= WIDTH || ny >= WIDTH || nz >= WIDTH) || !hasVoxel(nx, ny, nz)) {
                    return false;
                }
            }
            return true;
        };

        for (const auto& [x, y, z] : m_litVoxels) {
            if (isSurrounded(x, y, z)) continue;
            const VoxelData& voxel = getVoxel(x, y, z);
            assert(!voxel.isEmpty() && voxel.getBlock().emitsLight());
            lights.emplace_back(
                static_cast<int>(x) + xOffset * static_cast<int>(WIDTH),
                static_cast<int>(y) + yOffset * static_cast<int>(WIDTH),
                static_cast<int>(z) + zOffset * static_cast<int>(WIDTH),
                voxel.getBlock().getLightInfo()
            );
        }
        return lights;
    }
private:
    glm::ivec3 m_position{};
    SparseVoxelOctTree m_data{};
    bool m_gpuBufferDirty{true};
    bool m_saveFileDirty{false};
    std::chrono::steady_clock::time_point m_lastEdit;

    std::vector<QueuedVoxelOp> m_queuedVoxelsToSet;

    struct LightPos {
        uint8_t x,y,z;

        template <typename H>
        friend H AbslHashValue(H h, const LightPos& v) {
            return H::combine(std::move(h), v.x, v.y, v.z);
        }
        bool operator==(const LightPos& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    HashSet<LightPos> m_litVoxels;

    void setVoxelInternal(const VoxelData& voxel, const uint32_t x, const uint32_t y, const uint32_t z) {
        m_data.setVoxel(x, y, z, voxel);
        if (voxel.getBlock().emitsLight()) {
            m_litVoxels.insert({static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(z)});
        } else {
            m_litVoxels.erase({static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(z)});
        }
    }

    [[nodiscard]] std::string getFileName() const {
        return std::to_string(m_position.x) + "."
            + std::to_string(m_position.y) + "."
            + std::to_string(m_position.z) + "."
            + std::to_string(WIDTH) + ".compressed.chunk";
    }
};
