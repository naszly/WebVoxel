#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

#include "ChunkData.h"
#include "../Utils.h"

// depth: depth of the tree
// size: size of the matrix of nodes
template<uint32_t depth, uint32_t size>
class SparseVoxelTree {
    static constexpr VoxelData EMPTY_VOXEL{};
public:
    SparseVoxelTree() {
        if constexpr (depth == 0) {
            std::fill_n(&children[0][0][0], size * size * size, EMPTY_VOXEL);
        } else {
            std::fill_n(&children[0][0][0], size * size * size, nullptr);
        }
    }

    ~SparseVoxelTree() {
        if constexpr (depth != 0) {
            for (uint32_t x = 0; x < size; x++) {
                for (uint32_t y = 0; y < size; y++) {
                    for (uint32_t z = 0; z < size; z++) {
                        if (children[x][y][z] != nullptr)
                            delete children[x][y][z];
                    }
                }
            }
        }
    }

    SparseVoxelTree(const SparseVoxelTree&) = delete;
    SparseVoxelTree& operator=(const SparseVoxelTree&) = delete;
    SparseVoxelTree(SparseVoxelTree&&) = delete;
    SparseVoxelTree& operator=(SparseVoxelTree&&) = delete;

    [[nodiscard]] const VoxelData& get(uint32_t x, uint32_t y, uint32_t z) const {
        if constexpr (depth == 0) {
            return children[x][y][z];
        } else {
            constexpr uint32_t s = Utils::pow(size, depth);
            if (children[x / s][y / s][z / s] == nullptr)
                return EMPTY_VOXEL;
            return children[x / s][y / s][z / s]->get(x % s, y % s, z % s);
        }
    }

    void set(uint32_t x, uint32_t y, uint32_t z, const VoxelData &voxel) {
        if constexpr (depth == 0) {
            assert(x < size && y < size && z < size);
            emptyCount += (voxel.isEmpty()) - (children[x][y][z].isEmpty());
            children[x][y][z] = voxel;
        } else {
            constexpr uint32_t s = Utils::pow(size, depth);
            assert(x / s < size && y / s < size && z / s < size);
            if (children[x / s][y / s][z / s] == nullptr) {
                if (voxel.isEmpty())
                    return;
                children[x / s][y / s][z / s] = new SparseVoxelTree<depth - 1, size>();
            }
            children[x / s][y / s][z / s]->set(x % s, y % s, z % s, voxel);
        }
    }

    [[nodiscard]] bool isEmpty() const {
        return countVoxels() == 0;
    }

    [[nodiscard]] uint32_t countVoxels() const {
        if constexpr (depth == 0) {
            return size * size * size - emptyCount;
        } else {
            uint32_t count = 0;
            for (uint32_t x = 0; x < size; x++) {
                for (uint32_t y = 0; y < size; y++) {
                    for (uint32_t z = 0; z < size; z++) {
                        if (children[x][y][z] != nullptr)
                            count += children[x][y][z]->countVoxels();
                    }
                }
            }
            return count;
        }
    }

    [[nodiscard]] std::vector<VertexData> getVertices(const int32_t levelOfDetail = depth) const {
        assert(levelOfDetail >= 0 && levelOfDetail <= depth);
        std::vector<VertexData> vertices;
        vertices.reserve(countVoxels());
        getVertices(vertices, levelOfDetail);
        return vertices;
    }

private:
    std::conditional_t<depth == 0, VoxelData, SparseVoxelTree<depth - 1, size> *> children[size][size][size];

    uint32_t emptyCount{size * size * size};

    friend class SparseVoxelTree<depth + 1, size>;

    static void iterateOverChildren(const std::function<void(uint32_t, uint32_t, uint32_t)>& func) {
        for (uint32_t x = 0; x < size; ++x) {
            for (uint32_t y = 0; y < size; ++y) {
                for (uint32_t z = 0; z < size; ++z) {
                    func(x, y, z);
                }
            }
        }
    }

    [[nodiscard]] VoxelData getAverage() const {
        if constexpr (depth == 0) {
            if (isEmpty())
                return EMPTY_VOXEL;

            VoxelData average{};

            for (uint32_t x = 0; x < size; x++) {
                for (uint32_t y = 0; y < size; y++) {
                    for (uint32_t z = 0; z < size; z++) {
                        average.r += children[x][y][z].r;
                        average.g += children[x][y][z].g;
                        average.b += children[x][y][z].b;
                        average.a += children[x][y][z].a;
                    }
                }
            }

            average.r /= size * size * size - emptyCount;
            average.g /= size * size * size - emptyCount;
            average.b /= size * size * size - emptyCount;
            average.a /= size * size * size - emptyCount;

            return average;
        } else {
            VoxelData average{};
            uint32_t count = 0;

            for (uint32_t x = 0; x < size; x++) {
                for (uint32_t y = 0; y < size; y++) {
                    for (uint32_t z = 0; z < size; z++) {
                        if (children[x][y][z] != nullptr) {
                            const VoxelData childAverage = children[x][y][z]->getAverage();
                            average.r += childAverage.r;
                            average.g += childAverage.g;
                            average.b += childAverage.b;
                            average.a += childAverage.a;
                            count++;
                        }
                    }
                }
            }

            if (count == 0)
                return EMPTY_VOXEL;

            average.r /= count;
            average.g /= count;
            average.b /= count;
            average.a /= count;

            return average;
        }
    }

    void getVertices(std::vector<VertexData> &vector, const int32_t levelOfDetail = depth, uint32_t ox = 0, uint32_t oy = 0, uint32_t oz = 0) const {
        if constexpr (depth == 0) {
            if (levelOfDetail < 0) {
                VertexData vertex;
                vertex.x = ox;
                vertex.y = oy;
                vertex.z = oz;
                vertex.w = size;
                vertex.voxel = getAverage();
                vector.push_back(vertex);
            } else {
                iterateOverChildren([&](uint32_t x, uint32_t y, uint32_t z) {
                    if (!children[x][y][z].isEmpty()) {
                        VertexData vertex;
                        vertex.x = x + ox;
                        vertex.y = y + oy;
                        vertex.z = z + oz;
                        vertex.w = 1;
                        vertex.voxel = children[x][y][z];
                        vector.push_back(vertex);
                    }
                });
            }
        } else {
            constexpr uint32_t s = Utils::pow(size, depth);
            if (levelOfDetail < 0) {
                VertexData vertex;
                vertex.x = ox;
                vertex.y = oy;
                vertex.z = oz;
                vertex.w = s * size;
                vertex.voxel = getAverage();
                vector.push_back(vertex);
            } else {
                iterateOverChildren([&](uint32_t x, uint32_t y, uint32_t z) {
                   if (children[x][y][z] != nullptr) {
                       children[x][y][z]->getVertices(vector, levelOfDetail - 1, ox + x * s, oy + y * s, oz + z * s);
                   }
               });
            }
        }
    }
};