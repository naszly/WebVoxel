#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>

class Camera {
public:
    Camera() = default;

    void setPerspective(float fov, float aspect, float near, float far) {
        m_projection = glm::perspective(fov, aspect, near, far);
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

private:
    glm::mat4 m_projection{1.0f};
    glm::vec3 m_direction{0.0f, 0.0f, 1.0f};
    glm::vec3 m_position{0.0f};
};
