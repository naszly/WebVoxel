#pragma once

#include <glm/vec3.hpp>
#include <vector>

#include "common/datastructures/Bitmap.h"
#include "application/world/chunk/Chunk.h"

class ChunkShallow {
public:
    static constexpr size_t WIDTH = Chunk::WIDTH;
    using BitmapT = Bitmap<WIDTH * WIDTH * WIDTH>;

    ChunkShallow() = default;

    explicit ChunkShallow(const Chunk& src)
        : m_position(src.getPosition()), m_bitmap(src.getBitmap()), m_lightSources(src.getLightSources()) {
    }

    [[nodiscard]] glm::ivec3 getPosition() const { return m_position; }
    [[nodiscard]] const BitmapT& getBitmap() const { return m_bitmap; }
    [[nodiscard]] bool isEmpty() const { return m_bitmap.isEmpty(); }

    std::vector<Chunk::LightSource> getLightSources(const int xOffset = 0, const int yOffset = 0, const int zOffset = 0) const {
        std::vector<Chunk::LightSource> out;
        out.reserve(m_lightSources.size());
        for (const auto& [x, y, z, lightInfo] : m_lightSources) {
            out.push_back({
                x + xOffset * static_cast<int>(WIDTH),
                y + yOffset * static_cast<int>(WIDTH),
                z + zOffset * static_cast<int>(WIDTH),
                lightInfo
            });
        }
        return out;
    }

    bool hasVoxel(const uint32_t bx, const uint32_t by, const uint32_t bz) const {
        if (bx >= WIDTH || by >= WIDTH || bz >= WIDTH) return false;
        const uint32_t index = bx * WIDTH * WIDTH + by * WIDTH + bz;
        return m_bitmap.test(index);
    }

private:
    glm::ivec3 m_position{};
    BitmapT m_bitmap{};
    std::vector<Chunk::LightSource> m_lightSources{};
};

