#include "LightPropagator.h"

#include <cassert>
#include <algorithm>
#include <ranges>

namespace {
    bool testBitmaps(const ChunkNeighborhood& neighborChunks, const uint32_t x, const uint32_t y, const uint32_t z) {
        assert(x < 3 * Chunk::WIDTH && y < 3 * Chunk::WIDTH && z < 3 * Chunk::WIDTH);
        const uint32_t cnx = x / Chunk::WIDTH;
        const uint32_t cny = y / Chunk::WIDTH;
        const uint32_t cnz = z / Chunk::WIDTH;
        const uint32_t bx = x % Chunk::WIDTH;
        const uint32_t by = y % Chunk::WIDTH;
        const uint32_t bz = z % Chunk::WIDTH;
        const uint32_t index = bx * Chunk::WIDTH * Chunk::WIDTH + by * Chunk::WIDTH + bz;
        return neighborChunks.getChunk(cnx, cny, cnz)->getBitmap().test(index);
    }
}

const LightPropagator::LightMap& LightPropagator::compute(const ChunkNeighborhood& neighborChunks,
                                                          const std::vector<Chunk::LightSource>& lights) {
    static std::array<std::vector<int>, 32> buckets; // up to 31
    for (auto& b : buckets) b.clear();

    struct Storage {  std::unique_ptr<LightMap> map; };
    thread_local Storage storage { std::make_unique<LightMap>() };
    auto& lightMap = *storage.map;
    std::ranges::fill(lightMap, 0);

    if (lights.empty())
        return lightMap;

    constexpr int STRIDE_Y = DIM;
    constexpr int STRIDE_X = DIM * DIM;
    constexpr uint8_t MAX_L = 31;

    struct Dir { int dx, dy, dz; };
    static constexpr auto DIRS = std::array<Dir,6>{{
        { 1, 0, 0},{-1, 0, 0},
        { 0, 1, 0},{ 0,-1, 0},
        { 0, 0, 1},{ 0, 0,-1},
    }};

    static constexpr std::array<int, DIRS.size()> OFFSETS = []{
        std::array<int, DIRS.size()> offs{};
        for (size_t i=0;i<DIRS.size();++i) {
            offs[i] = DIRS[i].dx * STRIDE_X + DIRS[i].dy * STRIDE_Y + DIRS[i].dz;
        }
        return offs;
    }();

    auto toIndex = [&](const int x, const int y, const int z){ return x * STRIDE_X + y * STRIDE_Y + z; };

    for (const auto& [x, y, z, lightInfo] : lights) {
        const int gx = x + Chunk::WIDTH;
        const int gy = y + Chunk::WIDTH;
        const int gz = z + Chunk::WIDTH;

        assert(static_cast<unsigned>(gx) < DIM && static_cast<unsigned>(gy) < DIM && static_cast<unsigned>(gz) < DIM);

        const uint8_t l = std::min<uint8_t>(lightInfo.getIntensity(), MAX_L);
        const int idx = toIndex(gx,gy,gz);
        if (l > lightMap[idx]) {
            lightMap[idx] = l;
            buckets[l].push_back(idx);
        }
    }

    if (std::ranges::all_of(lightMap, [](const uint8_t v){ return v==0; }))
        return lightMap;

    for (int level = MAX_L; level > 0; --level) {
        auto& bucket = buckets[level];
        for (size_t i = 0; i < bucket.size(); ++i) {
            const int idx = bucket[i];
            if (lightMap[idx] != level)
                continue;

            const int x = idx / STRIDE_X;
            const int y = (idx - x * STRIDE_X) / STRIDE_Y;
            const int z = idx - x * STRIDE_X - y * STRIDE_Y;

            const uint8_t next = static_cast<uint8_t>(level - 1);

            for (size_t d = 0; d < DIRS.size(); ++d) {
                const auto& [dx, dy, dz] = DIRS[d];
                const int nx = x + dx;
                const int ny = y + dy;
                const int nz = z + dz;

                if (static_cast<unsigned>(nx) >= DIM ||
                    static_cast<unsigned>(ny) >= DIM ||
                    static_cast<unsigned>(nz) >= DIM)
                    continue;

                if (testBitmaps(neighborChunks, nx, ny, nz))
                    continue;

                int nextIdx = idx + OFFSETS[d];
                if (lightMap[nextIdx] < next) {
                    lightMap[nextIdx] = next;
                    if (next > 0)
                        buckets[next].push_back(nextIdx);
                }
            }
        }
    }

    return lightMap;
}
