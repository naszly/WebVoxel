#pragma once

#include "Chunk.h"
#include "ChunkMap.h"

class World {
public:
    World() = default;
    ~World() = default;

    World(const World&) = delete;
    World(World&&) = delete;
    World& operator=(const World&) = delete;
    World& operator=(World&&) = delete;

    void generate() {
        for (int i = 0; i < CHUNKS; i++) {
            for (int j = 0; j < CHUNKS; j++) {
                for (int k = 0; k < CHUNKS; k++) {
                    if ((i + j) % 2 == 0 && (j + k) % 2 == 0 && (i + k) % 2 == 0) {
                        m_Chunks.put(i, j, k);
                    }
                }
            }
        }
    }

    [[nodiscard]] std::vector<std::pair<glm::ivec3, const Chunk&>> getChunks() const {
        std::vector<std::pair<glm::ivec3, const Chunk&>> chunks;
        for (const auto& chunk : m_Chunks) {
            chunks.push_back(chunk);
        }
        return chunks;
    }

private:
    constexpr static int CHUNKS = 4;
    ChunkMap<Chunk, CHUNKS, CHUNKS, CHUNKS> m_Chunks;
};
