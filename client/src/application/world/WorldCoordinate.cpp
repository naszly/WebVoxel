#include "WorldCoordinate.h"
#include "chunk/Chunk.h"
#include "common/Utils.h"

constexpr auto CHUNK_SIZE = glm::i64vec3(Chunk::WIDTH);

static auto divideRoundDown(const glm::i64vec3 &numerator, const glm::i64vec3 &denominator) {
    return glm::i64vec3(Utils::divideRoundDown(numerator.x, denominator.x),
                        Utils::divideRoundDown(numerator.y, denominator.y),
                        Utils::divideRoundDown(numerator.z, denominator.z));
}

glm::i64vec3 WorldCoordinate::worldPosition() const {
    return m_position;
}

glm::ivec3 WorldCoordinate::chunkPosition() const {
    return divideRoundDown(m_position, CHUNK_SIZE);
}

glm::ivec3 WorldCoordinate::localPosition() const {
    return Utils::mod(m_position, CHUNK_SIZE);
}