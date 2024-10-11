
#include "ControllerSystem.h"

#include "../Application.h"

void ControllerSystem::initialize(const Window &) {
    Camera& camera = GetCamera();

    constexpr float fov = glm::radians(66.0);

    camera.setDirection({0,0,1});
    camera.setPerspective(fov, 1, 0.1, 1000);
    camera.setPosition({0,0,-5});
}

void ControllerSystem::render() {

}

void ControllerSystem::update(float dt) {
    const Input& input = GetInput();
    Camera& camera = GetCamera();

    const glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    auto position = camera.getPosition();
    float speed = dt * 50;

    if (input.isKeyPressed(KeyCode::W)) {
        position += camera.getDirection() * speed;
    }
    if (input.isKeyPressed(KeyCode::S)) {
        position -= camera.getDirection() * speed;
    }
    if (input.isKeyPressed(KeyCode::A)) {
        position += glm::normalize(glm::cross(camera.getDirection(), cameraUp)) * speed;
    }
    if (input.isKeyPressed(KeyCode::D)) {
        position -= glm::normalize(glm::cross(camera.getDirection(), cameraUp)) * speed;
    }
    if (input.isKeyPressed(KeyCode::Space)) {
        position += cameraUp * speed;
    }
    if (input.isKeyPressed(KeyCode::LeftShift)) {
        position -= cameraUp * speed;
    }

    camera.setPosition(position);
}

void ControllerSystem::onEvent(Event &event) {
    EventDispatcher dispatcher(event);

    dispatcher.dispatch<MouseMovedEvent>([&](const MouseMovedEvent &mouseEvent) {
        auto &camera = GetCamera();

        auto xPos = mouseEvent.getX();
        auto yPos = mouseEvent.getY();

        static double lastX = xPos;
        static double lastY = yPos;
        static bool firstMouse = true;

        if (firstMouse) {
            lastX = xPos;
            lastY = yPos;
            firstMouse = false;
        }

        double xOffset = lastX - xPos;
        double yOffset = lastY - yPos;
        lastX = xPos;
        lastY = yPos;

        double sensitivity = 0.05f;
        xOffset *= sensitivity;
        yOffset *= sensitivity;

        static double yaw = 90.0f;
        static double pitch = 0.0f;

        yaw += xOffset;
        pitch += yOffset;

        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        glm::dvec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        camera.setDirection(glm::normalize(front));
        return true;
    });
}
