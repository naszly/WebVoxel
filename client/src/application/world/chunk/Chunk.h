#pragma once

#include <cassert>
#include <random>
#include <glm/vec3.hpp>

#include "ChunkNeighbours.h"
#include "BitmappedVoxelTree.h"
#include "common/Utils.h"

#include <FastNoise/FastNoise.h>

class Chunk {
    static constexpr size_t TREE_DEPTH = 6;
    static constexpr size_t NODE_COUNT_PER_AXIS = 2;
    using SparseVoxelOctTree = BitmappedVoxelTree<TREE_DEPTH, NODE_COUNT_PER_AXIS>;
public:
    static constexpr size_t WIDTH = Utils::pow(NODE_COUNT_PER_AXIS, TREE_DEPTH);
    static_assert(WIDTH >= 16 && WIDTH <= 256, "SIZE must be between 16 and 256");

    explicit Chunk(const glm::ivec3 position)
        : m_position(position), m_lastAccess(std::chrono::steady_clock::now()) {}

    explicit Chunk(const int x, const int y, const int z)
        : m_position(x, y, z), m_lastAccess(std::chrono::steady_clock::now()) {}

    ~Chunk() = default;

    Chunk(const Chunk& other) {
        m_position = other.m_position;
        m_data = other.m_data;
        m_gpuBufferDirty = other.m_gpuBufferDirty;
        m_saveFileDirty = other.m_saveFileDirty;
        m_lastAccess = other.m_lastAccess;
    }

    Chunk& operator=(const Chunk& other) {
        if (this != &other) {
            m_position = other.m_position;
            m_data = other.m_data;
            m_gpuBufferDirty = other.m_gpuBufferDirty;
            m_saveFileDirty = other.m_saveFileDirty;
            m_lastAccess = other.m_lastAccess;
        }
        return *this;
    }

    Chunk(Chunk&& other) noexcept {
        m_position = other.m_position;
        m_data = std::move(other.m_data);
        m_gpuBufferDirty = other.m_gpuBufferDirty;
        m_saveFileDirty = other.m_saveFileDirty;
        m_lastAccess = other.m_lastAccess;
        other.m_gpuBufferDirty = false;
        other.m_saveFileDirty = false;
    }

    Chunk& operator=(Chunk&& other) noexcept {
        if (this != &other) {
            m_position = other.m_position;
            m_data = std::move(other.m_data);
            m_gpuBufferDirty = other.m_gpuBufferDirty;
            m_saveFileDirty = other.m_saveFileDirty;
            m_lastAccess = other.m_lastAccess;
            other.m_gpuBufferDirty = false;
            other.m_saveFileDirty = false;
        }
        return *this;
    }

    void generate(const FastNoise::SmartNode<> &fnGenerator);

    [[nodiscard]] glm::ivec3 getPosition() const {
        return m_position;
    }

    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        m_lastAccess = std::chrono::steady_clock::now();
        return m_data.getVoxel(x, y, z);
    }

    [[nodiscard]] bool hasVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        m_lastAccess = std::chrono::steady_clock::now();
        return m_data.hasVoxel(x, y, z);
    }

    void setVoxel(const VoxelData& voxel, const uint32_t x, const uint32_t y, const uint32_t z) {
        m_data.setVoxel(x, y, z, voxel);
        m_gpuBufferDirty = true;
        m_saveFileDirty = true;
        m_lastAccess = std::chrono::steady_clock::now();
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
        m_lastAccess = std::chrono::steady_clock::now();
        return m_data.getBitmap(getNeighbours(chunkNeighbours));
    }

    [[nodiscard]] auto getBitmap(const ExtendedChukNeighbours& chunkNeighbours) const {
        m_lastAccess = std::chrono::steady_clock::now();
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

    std::chrono::steady_clock::time_point getLastAccess() const {
        return m_lastAccess;
    }

    [[nodiscard]] std::chrono::duration<double> getLastAccessDuration() const {
        return std::chrono::steady_clock::now() - getLastAccess();
    }

    static void cleanFs();
private:
    glm::ivec3 m_position{};
    SparseVoxelOctTree m_data{};
    bool m_gpuBufferDirty{true};
    bool m_saveFileDirty{false};
    mutable std::chrono::steady_clock::time_point m_lastAccess;

    [[nodiscard]] std::string getFileName() const {
        return std::to_string(m_position.x) + "."
            + std::to_string(m_position.y) + "."
            + std::to_string(m_position.z) + "."
            + std::to_string(WIDTH) + ".compressed.chunk";
    }

    static std::optional<SparseVoxelOctTree::Neighbours> getNeighbours(const ChunkNeighbours &chunkNeighbours);

    static std::optional<SparseVoxelOctTree::ExtendedNeighbours> getNeighbours(const ExtendedChukNeighbours &chunkNeighbours);
};
