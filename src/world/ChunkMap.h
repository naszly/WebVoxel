#pragma once
#include <cassert>
#include <optional>
#include <glm/vec3.hpp>

template <typename T, int SIZE_X, int SIZE_Y, int SIZE_Z>
class ChunkMap {
public:
    ChunkMap() = default;
    ~ChunkMap() = default;

    ChunkMap(const ChunkMap&) = delete;
    ChunkMap(ChunkMap&&) = delete;
    ChunkMap& operator=(const ChunkMap&) = delete;
    ChunkMap& operator=(ChunkMap&&) = delete;

    void put(const int x, const int y, const int z) {
        assert(x >= 0 && x < SIZE_X);
        assert(y >= 0 && y < SIZE_Y);
        assert(z >= 0 && z < SIZE_Z);

        m_Chunks[x + SIZE_X * (y + SIZE_Y * z)] = std::make_optional<T>();

        m_Chunks[x + SIZE_X * (y + SIZE_Y * z)]->generate(x, y, z);
        m_Chunks[x + SIZE_X * (y + SIZE_Y * z)]->createVertexBuffer(x, y, z);
    }

    void remove(const int x, const int y, const int z) {
        assert(x >= 0 && x < SIZE_X);
        assert(y >= 0 && y < SIZE_Y);
        assert(z >= 0 && z < SIZE_Z);

        m_Chunks[x + SIZE_X * (y + SIZE_Y * z)] = std::nullopt;
    }

    class ConstIterator {
    public:
        ConstIterator(const std::optional<T>* ptr, const std::optional<T>* end, const std::optional<T>* chunks)
            : m_Ptr(ptr), m_End(end), m_Chunks(chunks) {
            // Skip to the first valid element
            while (m_Ptr != m_End && !m_Ptr->has_value()) {
                ++m_Ptr;
            }
        }

        std::pair<glm::ivec3, const T&> operator*() const {
            glm::ivec3 pos;

            pos.x = (m_Ptr - m_Chunks) % SIZE_X;
            pos.y = ((m_Ptr - m_Chunks) / SIZE_X) % SIZE_Y;
            pos.z = (m_Ptr - m_Chunks) / (SIZE_X * SIZE_Y);

            std::pair<glm::ivec3, const T&> pair = {pos, m_Ptr->value()};

            return pair;
        }

        ConstIterator& operator++() {
            do {
                ++m_Ptr;
            } while (m_Ptr != m_End && !m_Ptr->has_value());
            return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const ConstIterator& a, const ConstIterator& b) {
            return a.m_Ptr == b.m_Ptr;
        }

        friend bool operator!=(const ConstIterator& a, const ConstIterator& b) {
            return a.m_Ptr != b.m_Ptr;
        }

    private:
        const std::optional<T>* m_Ptr;
        const std::optional<T>* m_End;
        const std::optional<T>* m_Chunks;
    };

    ConstIterator begin() const {
        return ConstIterator(m_Chunks, m_Chunks + MAP_SIZE, m_Chunks);
    }

    ConstIterator end() const {
        return ConstIterator(m_Chunks + MAP_SIZE, m_Chunks + MAP_SIZE, m_Chunks);
    }

private:
    static constexpr size_t MAP_SIZE = SIZE_X * SIZE_Y * SIZE_Z;
    std::optional<T> m_Chunks[MAP_SIZE];
};
