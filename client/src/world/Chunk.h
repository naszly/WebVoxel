#pragma once

#include <cassert>
#include <functional>
#include <random>
#include <glm/vec3.hpp>

#include "SparseVoxelTree.h"
#include "../Utils.h"
#include "../WebGPUContext.h"

#include <FastNoise/FastNoise.h>

class Chunk;

class ChunkNeighbours {
public:
    ChunkNeighbours(const Chunk *x_minus, const Chunk *x_plus, const Chunk *y_minus, const Chunk *y_plus,
        const Chunk *z_minus, const Chunk *z_plus)
        : xMinus(x_minus),
          xPlus(x_plus),
          yMinus(y_minus),
          yPlus(y_plus),
          zMinus(z_minus),
          zPlus(z_plus) {}

    const Chunk* xMinus{nullptr};
    const Chunk* xPlus{nullptr};
    const Chunk* yMinus{nullptr};
    const Chunk* yPlus{nullptr};
    const Chunk* zMinus{nullptr};
    const Chunk* zPlus{nullptr};

    [[nodiscard]] bool hasAllNeighbours() const {
        return xMinus && xPlus && yMinus && yPlus && zMinus && zPlus;
    }
};

class ExtendedChukNeighbours : public ChunkNeighbours {
public:
    ExtendedChukNeighbours(const Chunk *x_minus, const Chunk *x_plus, const Chunk *y_minus, const Chunk *y_plus,
        const Chunk *z_minus, const Chunk *z_plus, const Chunk *x_minus_y_minus, const Chunk *x_minus_y_plus,
        const Chunk *x_minus_z_minus, const Chunk *x_minus_z_plus, const Chunk *x_plus_y_minus,
        const Chunk *x_plus_y_plus, const Chunk *x_plus_z_minus, const Chunk *x_plus_z_plus,
        const Chunk *y_minus_z_minus, const Chunk *y_minus_z_plus, const Chunk *y_plus_z_minus,
        const Chunk *y_plus_z_plus, const Chunk *x_minus_y_minus_z_minus, const Chunk *x_minus_y_minus_z_plus,
        const Chunk *x_minus_y_plus_z_minus, const Chunk *x_minus_y_plus_z_plus, const Chunk *x_plus_y_minus_z_minus,
        const Chunk *x_plus_y_minus_z_plus, const Chunk *x_plus_y_plus_z_minus, const Chunk *x_plus_y_plus_z_plus)
        : ChunkNeighbours(x_minus, x_plus, y_minus, y_plus, z_minus, z_plus),
          xMinusYMinus(x_minus_y_minus),
          xMinusYPlus(x_minus_y_plus),
          xMinusZMinus(x_minus_z_minus),
          xMinusZPlus(x_minus_z_plus),
          xPlusYMinus(x_plus_y_minus),
          xPlusYPlus(x_plus_y_plus),
          xPlusZMinus(x_plus_z_minus),
          xPlusZPlus(x_plus_z_plus),
          yMinusZMinus(y_minus_z_minus),
          yMinusZPlus(y_minus_z_plus),
          yPlusZMinus(y_plus_z_minus),
          yPlusZPlus(y_plus_z_plus),
          xMinusYMinusZMinus(x_minus_y_minus_z_minus),
          xMinusYMinusZPlus(x_minus_y_minus_z_plus),
          xMinusYPlusZMinus(x_minus_y_plus_z_minus),
          xMinusYPlusZPlus(x_minus_y_plus_z_plus),
          xPlusYMinusZMinus(x_plus_y_minus_z_minus),
          xPlusYMinusZPlus(x_plus_y_minus_z_plus),
          xPlusYPlusZMinus(x_plus_y_plus_z_minus),
          xPlusYPlusZPlus(x_plus_y_plus_z_plus) {}

    const Chunk* xMinusYMinus{nullptr};
    const Chunk* xMinusYPlus{nullptr};
    const Chunk* xMinusZMinus{nullptr};
    const Chunk* xMinusZPlus{nullptr};
    const Chunk* xPlusYMinus{nullptr};
    const Chunk* xPlusYPlus{nullptr};
    const Chunk* xPlusZMinus{nullptr};
    const Chunk* xPlusZPlus{nullptr};
    const Chunk* yMinusZMinus{nullptr};
    const Chunk* yMinusZPlus{nullptr};
    const Chunk* yPlusZMinus{nullptr};
    const Chunk* yPlusZPlus{nullptr};
    const Chunk* xMinusYMinusZMinus{nullptr};
    const Chunk* xMinusYMinusZPlus{nullptr};
    const Chunk* xMinusYPlusZMinus{nullptr};
    const Chunk* xMinusYPlusZPlus{nullptr};
    const Chunk* xPlusYMinusZMinus{nullptr};
    const Chunk* xPlusYMinusZPlus{nullptr};
    const Chunk* xPlusYPlusZMinus{nullptr};
    const Chunk* xPlusYPlusZPlus{nullptr};

    [[nodiscard]] bool hasAllNeighbours() const {
        return xMinus && xPlus && yMinus && yPlus && zMinus && zPlus &&
               xMinusYMinus && xMinusYPlus && xMinusZMinus && xMinusZPlus &&
               xPlusYMinus && xPlusYPlus && xPlusZMinus && xPlusZPlus &&
               yMinusZMinus && yMinusZPlus && yPlusZMinus && yPlusZPlus &&
               xMinusYMinusZMinus && xMinusYMinusZPlus && xMinusYPlusZMinus && xMinusYPlusZPlus &&
               xPlusYMinusZMinus && xPlusYMinusZPlus && xPlusYPlusZMinus && xPlusYPlusZPlus;
    }
};

class Chunk {
    static constexpr size_t DEPTH = 6;
    static constexpr size_t NODE_SIZE = 2;
    using SparseVoxelOctTree = SparseVoxelTree<DEPTH, NODE_SIZE>;
public:
    static constexpr size_t SIZE = Utils::pow(NODE_SIZE, DEPTH);
    static_assert(SIZE >= 16 && SIZE <= 256, "SIZE must be between 16 and 256");

    explicit Chunk(const glm::ivec3 position) : m_Position(position) {}
    explicit Chunk(const int x, const int y, const int z) : m_Position(x, y, z) {}

    ~Chunk() = default;

    Chunk(const Chunk&) = delete;

    Chunk& operator=(const Chunk&) = delete;

    Chunk(Chunk&& other) noexcept {
        m_Position = other.m_Position;
        m_Data = std::move(other.m_Data);
        m_Dirty = other.m_Dirty;
    }

    Chunk& operator=(Chunk&& other) noexcept {
        if (this != &other) {
            m_Position = other.m_Position;
            m_Data = std::move(other.m_Data);
            m_Dirty = other.m_Dirty;
        }
        return *this;
    }

    void generate(const FastNoise::SmartNode<> &fnGenerator);

    [[nodiscard]] glm::ivec3 getPosition() const {
        return m_Position;
    }

    [[nodiscard]] const VoxelData& getVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return m_Data.getVoxel(x, y, z);
    }

    [[nodiscard]] bool hasVoxel(const uint32_t x, const uint32_t y, const uint32_t z) const {
        return m_Data.hasVoxel(x, y, z);
    }

    void setVoxel(const VoxelData& voxel, const uint32_t x, const uint32_t y, const uint32_t z) {
        m_Data.setVoxel(x, y, z, voxel);
        m_Dirty = true;
    }

    [[nodiscard]] bool isDirty() const {
        return m_Dirty;
    }

    void setDirty() {
        m_Dirty = true;
    }

    void resetDirty() {
        m_Dirty = false;
    }

    [[nodiscard]] auto getBitmap(const ChunkNeighbours& chunkNeighbours) const {
        return m_Data.getBitmap(getNeighbours(chunkNeighbours));
    }

    [[nodiscard]] bool fileExists() const;

    void save() const;

    void load();

    static void CleanFs();
private:
    glm::ivec3 m_Position{};
    SparseVoxelOctTree m_Data{};
    bool m_Dirty{true};

    [[nodiscard]] std::string getFileName() const {
        return std::to_string(m_Position.x) + "."
            + std::to_string(m_Position.y) + "."
            + std::to_string(m_Position.z) + "."
            + std::to_string(SIZE) + ".chunk";
    }

    static std::optional<SparseVoxelOctTree::Neighbours> getNeighbours(const ChunkNeighbours &chunkNeighbours);

    static std::optional<SparseVoxelOctTree::ExtendedNeighbours> getNeighbours(const ExtendedChukNeighbours &chunkNeighbours);
};
