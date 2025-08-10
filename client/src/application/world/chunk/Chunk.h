#pragma once

#include <cassert>
#include <random>
#include <glm/vec3.hpp>
#include <vector>

#include "ChunkNeighbours.h"
#include "BitmappedVoxelTree.h"
#include "application/world/WorldGenerator.h"
#include "common/Utils.h"

class Chunk {
    static constexpr size_t TREE_DEPTH = 6;
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
    }

    Chunk& operator=(const Chunk& other) {
        if (this != &other) {
            m_position = other.m_position;
            m_data = other.m_data;
            m_gpuBufferDirty = other.m_gpuBufferDirty;
            m_saveFileDirty = other.m_saveFileDirty;
            m_lastEdit = other.m_lastEdit;
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
        m_data.setVoxel(x, y, z, voxel);
        m_gpuBufferDirty = true;
        m_saveFileDirty = true;
        m_lastEdit = std::chrono::steady_clock::now();
    }

    void queueVoxelToSet(const VoxelData& voxel, const uint32_t x, const uint32_t y, const uint32_t z) {
        m_queuedVoxelsToSet.emplace_back(glm::ivec3(x, y, z), voxel);
    }

    void executeQueuedVoxelsToSet() {
        for (const auto& [pos, voxel] : m_queuedVoxelsToSet) {
            m_data.setVoxel(pos.x, pos.y, pos.z, voxel);
        }
        m_queuedVoxelsToSet.resize(0);
        m_gpuBufferDirty = true;
        m_saveFileDirty = true;
        m_lastEdit = std::chrono::steady_clock::now();
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

    [[nodiscard]] auto getBitmap(const ChunkNeighbours& chunkNeighbours) const {
        return m_data.getBitmap(getNeighbours(chunkNeighbours));
    }

    [[nodiscard]] auto getBitmap(const ExtendedChukNeighbours& chunkNeighbours) const {
        return m_data.getBitmap(getNeighbours(chunkNeighbours));
    }

    [[nodiscard]] bool fileExists() const;

    void save();

    void load();

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
private:
    glm::ivec3 m_position{};
    SparseVoxelOctTree m_data{};
    bool m_gpuBufferDirty{true};
    bool m_saveFileDirty{false};
    std::chrono::steady_clock::time_point m_lastEdit;

    std::vector<QueuedVoxelOp> m_queuedVoxelsToSet;

    [[nodiscard]] std::string getFileName() const {
        return std::to_string(m_position.x) + "."
            + std::to_string(m_position.y) + "."
            + std::to_string(m_position.z) + "."
            + std::to_string(WIDTH) + ".compressed.chunk";
    }

    static std::optional<SparseVoxelOctTree::Neighbours> getNeighbours(const ChunkNeighbours &chunkNeighbours);

    static std::optional<SparseVoxelOctTree::ExtendedNeighbours> getNeighbours(const ExtendedChukNeighbours &chunkNeighbours);
};
