#pragma once

#include <glm/glm.hpp>

class WorldCoordinate {
public:
    explicit WorldCoordinate(const glm::i64vec3 &position) : m_position(position) {}

    [[nodiscard]] glm::i64vec3 worldPosition() const;

    [[nodiscard]] glm::ivec3 chunkPosition() const;

    [[nodiscard]] glm::ivec3 localPosition() const;
private:
    glm::i64vec3 m_position;
};