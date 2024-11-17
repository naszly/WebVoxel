#include "World.h"

void World::generate() {
    for (int i = 0; i < CHUNKS; i++) {
        for (int j = 0; j < CHUNKS; j++) {
            for (int k = 0; k < CHUNKS; k++) {
                if ((i + j) % 2 == 0 && (j + k) % 2 == 0 && (i + k) % 2 == 0) {
                    m_Chunks.get(i, j, k).generate(i, j, k);
                }
            }
        }
    }
}

std::vector<std::pair<glm::ivec3, Chunk &>> World::getChunks() {
    std::vector<std::pair<glm::ivec3, Chunk&>> chunks;
    for (auto& c : m_Chunks) {
        if (!c.has_value()) {
            continue;
        }
        Chunk& chunk = c.value();
        chunks.emplace_back(chunk.getPosition(), chunk);
    }
    return chunks;
}
