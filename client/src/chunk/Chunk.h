#pragma once

#include <cassert>
#include <random>
#include <glm/vec3.hpp>

#include "VoxelTree.h"
#include "../Utils.h"

#include <FastNoise/FastNoise.h>

class Chunk;

class ChunkNeighbours {
public:
    ChunkNeighbours(const Chunk *xMinus, const Chunk *xPlus, const Chunk *yMinus, const Chunk *yPlus,
        const Chunk *zMinus, const Chunk *zPlus)
        : xMinus(xMinus),
          xPlus(xPlus),
          yMinus(yMinus),
          yPlus(yPlus),
          zMinus(zMinus),
          zPlus(zPlus) {}

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
    ExtendedChukNeighbours(const Chunk *xMinus, const Chunk *xPlus, const Chunk *yMinus, const Chunk *yPlus,
        const Chunk *zMinus, const Chunk *zPlus, const Chunk *xMinusYMinus, const Chunk *xMinusYPlus,
        const Chunk *xMinusZMinus, const Chunk *xMinusZPlus, const Chunk *xPlusYMinus,
        const Chunk *xPlusYPlus, const Chunk *xPlusZMinus, const Chunk *xPlusZPlus,
        const Chunk *yMinusZMinus, const Chunk *yMinusZPlus, const Chunk *yPlusZMinus,
        const Chunk *yPlusZPlus, const Chunk *xMinusYMinusZMinus, const Chunk *xMinusYMinusZPlus,
        const Chunk *xMinusYPlusZMinus, const Chunk *xMinusYPlusZPlus, const Chunk *xPlusYMinusZMinus,
        const Chunk *xPlusYMinusZPlus, const Chunk *xPlusYPlusZMinus, const Chunk *xPlusYPlusZPlus)
        : ChunkNeighbours(xMinus, xPlus, yMinus, yPlus, zMinus, zPlus),
          xMinusYMinus(xMinusYMinus),
          xMinusYPlus(xMinusYPlus),
          xMinusZMinus(xMinusZMinus),
          xMinusZPlus(xMinusZPlus),
          xPlusYMinus(xPlusYMinus),
          xPlusYPlus(xPlusYPlus),
          xPlusZMinus(xPlusZMinus),
          xPlusZPlus(xPlusZPlus),
          yMinusZMinus(yMinusZMinus),
          yMinusZPlus(yMinusZPlus),
          yPlusZMinus(yPlusZMinus),
          yPlusZPlus(yPlusZPlus),
          xMinusYMinusZMinus(xMinusYMinusZMinus),
          xMinusYMinusZPlus(xMinusYMinusZPlus),
          xMinusYPlusZMinus(xMinusYPlusZMinus),
          xMinusYPlusZPlus(xMinusYPlusZPlus),
          xPlusYMinusZMinus(xPlusYMinusZMinus),
          xPlusYMinusZPlus(xPlusYMinusZPlus),
          xPlusYPlusZMinus(xPlusYPlusZMinus),
          xPlusYPlusZPlus(xPlusYPlusZPlus) {}

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
    using SparseVoxelOctTree = VoxelTree<DEPTH, NODE_SIZE>;
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

    [[nodiscard]] auto getBitmap(const ExtendedChukNeighbours& chunkNeighbours) const {
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
