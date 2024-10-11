#pragma once

#include <cstdint>
#include <functional>
#include <vector>

struct Voxel {
    uint8_t r{0}, g{0}, b{0}, a{0};

    [[nodiscard]] bool isEmpty() const {
        return r == 0 && g == 0 && b == 0 && a == 0;
    }
};

struct Vertex {
    uint8_t x{}, y{}, z{}, w{};
    Voxel voxel;
};

template <typename T, int SIZE_X, int SIZE_Y, int SIZE_Z>
class Chunk {
public:
    Chunk() = default;
    ~Chunk() = default;

    Chunk(const Chunk&) = delete;
    Chunk(Chunk&&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk& operator=(Chunk&&) = delete;

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

     [[nodiscard]] std::vector<Vertex> getPoints() const {
        std::vector<Vertex> points;
        points.reserve(SIZE_X * SIZE_Y * SIZE_Z);
        for (int x = 0; x < SIZE_X; x++) {
            for (int y = 0; y < SIZE_Y; y++) {
                for (int z = 0; z < SIZE_Z; z++) {
                    if (!m_Data[x][y][z].isEmpty()) {
                        Vertex vertex;
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