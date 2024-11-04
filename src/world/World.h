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
        m_Chunks.put(1,1,1);
        m_Chunks.put(0,0,0);
        m_Chunks.put(2,0,0);
        m_Chunks.put(0,2,0);
        m_Chunks.put(0,0,2);
        m_Chunks.put(2,2,0);
        m_Chunks.put(0,2,2);
        m_Chunks.put(2,0,2);
        m_Chunks.put(2,2,2);
    }

    [[nodiscard]] std::vector<std::pair<glm::ivec3, const Chunk&>> getChunks() const {
        std::vector<std::pair<glm::ivec3, const Chunk&>> chunks;
        for (const auto& chunk : m_Chunks) {
            chunks.push_back(chunk);
        }
        return chunks;
    }

private:
    ChunkMap<Chunk, 16, 16, 16> m_Chunks;
};
