#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>

class Camera {
public:
    Camera() = default;
    ~Camera() = default;

    constexpr static float DEFAULT_FOV = glm::radians(66.0f);
    constexpr static float NEAR = 0.01f;
    constexpr static float FAR = 1200.0f;

    void setPerspective(const float fov, const float aspect) {
        m_fov = fov;
        m_aspect = aspect;
        m_projection = glm::perspective(fov, aspect, NEAR, FAR);
    }

    void setDirection(const glm::vec3 &direction) {
        m_direction = direction;
    }

    void setPosition(const glm::vec3 &position) {
        m_position = position;
    }

    void setFov(const float fov) {
        setPerspective(fov, m_aspect);
    }

    void setAspect(const float aspect) {
        setPerspective(m_fov, aspect);
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

    [[nodiscard]] float getFov() const {
        return m_fov;
    }

    [[nodiscard]] float getAspect() const {
        return m_aspect;
    }

private:
    glm::mat4 m_projection{1.0f};
    glm::vec3 m_direction{0.0f, 0.0f, 1.0f};
    glm::vec3 m_position{0.0f};
    float m_fov{DEFAULT_FOV};
    float m_aspect{0.0f};
};