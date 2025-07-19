#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <array>
#include <algorithm>

class Camera {
public:
    Camera() = default;
    ~Camera() = default;

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    constexpr static float NEAR = 0.01f;
    constexpr static float FAR = 4000.0f;

    void setPerspective(const float fov, const float aspect) {
        m_projection = glm::perspective(fov, aspect, NEAR, FAR);
    }

    void setDirection(const glm::vec3 &direction) {
        m_direction = direction;
    }

    void setPosition(const glm::vec3 &position) {
        m_position = position;
    }

    [[nodiscard]] glm::mat4 getProjectionViewMatrix() const {
        return m_projection * glm::lookAt(glm::vec3(0), m_direction, glm::vec3(0, 1, 0));
    }

    [[nodiscard]] glm::mat4 getInverseProjectionViewMatrix() const {
        return glm::inverse(m_projection * glm::lookAt(glm::vec3(0), m_direction, glm::vec3(0, 1, 0)));
    }

    [[nodiscard]] glm::vec3 getDirection() const {
        return m_direction;
    }

    [[nodiscard]] glm::vec3 getPosition() const {
        return m_position;
    }

    [[nodiscard]] bool isSphereInFrustum(const glm::vec3& center, float radius) const {
        glm::mat4 projView = getProjectionViewMatrix();

        // Extract frustum planes from the projection-view matrix
        std::array planes{
            glm::vec4(projView[0][3] + projView[0][2], projView[1][3] + projView[1][2], projView[2][3] + projView[2][2], projView[3][3] + projView[3][2]), // Near
            glm::vec4(projView[0][3] - projView[0][2], projView[1][3] - projView[1][2], projView[2][3] - projView[2][2], projView[3][3] - projView[3][2]), // Far
            glm::vec4(projView[0][3] + projView[0][0], projView[1][3] + projView[1][0], projView[2][3] + projView[2][0], projView[3][3] + projView[3][0]), // Left
            glm::vec4(projView[0][3] - projView[0][0], projView[1][3] - projView[1][0], projView[2][3] - projView[2][0], projView[3][3] - projView[3][0]), // Right
            glm::vec4(projView[0][3] - projView[0][1], projView[1][3] - projView[1][1], projView[2][3] - projView[2][1], projView[3][3] - projView[3][1]), // Top
            glm::vec4(projView[0][3] + projView[0][1], projView[1][3] + projView[1][1], projView[2][3] + projView[2][1], projView[3][3] + projView[3][1]), // Bottom
        };

        // Normalize the planes
        for (auto& plane : planes) {
            const float length = glm::length(glm::vec3(plane));
            plane /= length;
        }

        // Sphere-Frustum Intersection Test
        return std::ranges::all_of(planes, [&](const glm::vec4& plane) {
            const float distance = glm::dot(glm::vec3(plane), center - m_position) + plane.w;
            return distance >= -radius;
        });
    }

private:
    glm::mat4 m_projection{1.0f};
    glm::vec3 m_direction{0.0f, 0.0f, 1.0f};
    glm::vec3 m_position{0.0f};
};