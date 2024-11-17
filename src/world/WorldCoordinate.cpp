#include "WorldCoordinate.h"
#include "Chunk.h"
#include "../Utils.h"

constexpr auto CHUNK_SIZE = glm::i64vec3(Chunk::SIZE);

static auto divideRoundDown(const glm::i64vec3 &numerator, const glm::i64vec3 &denominator) {
    return glm::i64vec3(Utils::divideRoundDown(numerator.x, denominator.x),
                        Utils::divideRoundDown(numerator.y, denominator.y),
                        Utils::divideRoundDown(numerator.z, denominator.z));
}

glm::i64vec3 WorldCoordinate::worldPosition() const {
    return position;
}

glm::ivec3 WorldCoordinate::chunkPosition() const {
    return divideRoundDown(position, CHUNK_SIZE);
}

glm::ivec3 WorldCoordinate::localPosition() const {
    return Utils::mod(position, CHUNK_SIZE);
}