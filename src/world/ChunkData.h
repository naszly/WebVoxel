#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <glm/vec4.hpp>

struct VoxelData {
    uint8_t r{0}, g{0}, b{0}, a{0};

    VoxelData() = default;
    VoxelData(const uint8_t r, const uint8_t g, const uint8_t b) : r(r), g(g), b(b), a(255) {}
    VoxelData(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a) : r(r), g(g), b(b), a(a) {}
    explicit VoxelData(const glm::vec4 color) : r(color.r * 255), g(color.g * 255), b(color.b * 255), a(color.a * 255) {}

    [[nodiscard]] bool isEmpty() const {
        return a == 0;
    }
};

struct VertexData {
    uint8_t x{}, y{}, z{}, w{};
    VoxelData voxel;
};

template <typename T, int SIZE_X, int SIZE_Y, int SIZE_Z>
class ChunkData {
public:
    ChunkData() = default;
    ~ChunkData() = default;

    T& operator()(const int x, const int y, const int z) {
        return m_Data[x][y][z];
    }

    const T& operator()(const int x, const int y, const int z) const {
        return m_Data[x][y][z];
    }

    T& get(const int x, const int y, const int z) {
        return m_Data[x][y][z];
    }

    const T& get(const int x, const int y, const int z) const {
        return m_Data[x][y][z];
    }

    void set(const int x, const int y, const int z, const T& value) {
        m_Data[x][y][z] = value;
    }

    void fill(const T& value) {
        for (int x = 0; x < SIZE_X; x++) {
            for (int y = 0; y < SIZE_Y; y++) {
                for (int z = 0; z < SIZE_Z; z++) {
                    m_Data[x][y][z] = value;
                }
            }
        }
    }

    void fill(const std::function<T(int, int, int)>& func) {
        for (int x = 0; x < SIZE_X; x++) {
            for (int y = 0; y < SIZE_Y; y++) {
                for (int z = 0; z < SIZE_Z; z++) {
                    m_Data[x][y][z] = func(x, y, z);
                }
            }
        }
    }

    void clear() {
        fill(T{});
    }

     [[nodiscard]] std::vector<VertexData> getPoints() const {
        std::vector<VertexData> points;
        points.reserve(SIZE_X * SIZE_Y * SIZE_Z);
        for (int x = 0; x < SIZE_X; x++) {
            for (int y = 0; y < SIZE_Y; y++) {
                for (int z = 0; z < SIZE_Z; z++) {
                    if (!m_Data[x][y][z].isEmpty()) {
                        VertexData vertex;
                        vertex.x = x;
                        vertex.y = y;
                        vertex.z = z;
                        vertex.voxel = m_Data[x][y][z];
                        points.push_back(vertex);
                    }
                }
            }
        }
        points.shrink_to_fit();
        return points;
    }

private:
    T m_Data[SIZE_X][SIZE_Y][SIZE_Z];
};
