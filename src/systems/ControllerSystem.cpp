
#include "ControllerSystem.h"

#include "../Application.h"
#include "../MouseEvent.h"

void ControllerSystem::initialize() {
    Camera& camera = GetCamera();

    camera.setDirection({0,0,1});
    camera.setPosition({0,0,-5});
}

void ControllerSystem::render() {

}

void ControllerSystem::update(const float dt) {
    const Input& input = GetInput();
    Camera& camera = GetCamera();
    World& world = GetWorld();

    updateCamera(dt, input, camera);
    updateWorld(dt, input, camera, world);
}

void ControllerSystem::onEvent(Event &event) {
    EventDispatcher dispatcher(event);

    dispatcher.dispatch<MouseMovedEvent>([&](const MouseMovedEvent &mouseEvent) {
        auto &camera = GetCamera();

        const auto xPos = mouseEvent.getX();
        const auto yPos = mouseEvent.getY();

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

        constexpr double sensitivity = 0.05f;
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

void ControllerSystem::updateCamera(const float dt, const Input &input, Camera &camera) {
    constexpr auto cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    auto position = camera.getPosition();
    const float speed = dt * 50;

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

typedef std::function<bool(glm::i64vec3, glm::i64vec3)> RayHitCallbackFn;

void castRay(glm::vec3 position, glm::vec3 direction, float length, const RayHitCallbackFn &callback) {
    glm::i64vec3 current = glm::floor(position);
    glm::i64vec3 end = glm::i64vec3(glm::floor(position + direction * length));
    glm::i64vec3 sign = glm::sign(direction);

    glm::vec3 tMax = (glm::vec3(current) + glm::step(glm::vec3(0), direction) - position) / direction;
    glm::vec3 tDelta = glm::vec3(sign) / direction;

    if (glm::isnan(tMax.x)) tMax.x = std::numeric_limits<float>::infinity();
    if (glm::isnan(tMax.y)) tMax.y = std::numeric_limits<float>::infinity();
    if (glm::isnan(tMax.z)) tMax.z = std::numeric_limits<float>::infinity();

    glm::i64vec3 previous = current;

    while (!callback(current, previous) && !(current == end))
    {
        previous = current;
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
            current.x += sign.x;
            tMax.x += tDelta.x;
        } else if (tMax.y < tMax.z) {
            current.y += sign.y;
            tMax.y += tDelta.y;
        } else {
            current.z += sign.z;
            tMax.z += tDelta.z;
        }
    }
}

void ControllerSystem::updateWorld(float dt, const Input &input, const Camera &camera, World &world) {
    static bool leftMousePressed = false;
    static bool rightMousePressed = false;

    const auto cameraPos = camera.getPosition();
    const auto cameraDir = glm::normalize(camera.getDirection());
    constexpr float reach = 100.0f;

    if (input.isMouseLeftButtonPressed()) {
        if (!leftMousePressed) {
            leftMousePressed = true;
            castRay(cameraPos, cameraDir, reach, [&](const auto &pos, const auto &prevPos) {
                const auto worldPos = WorldCoordinate(pos);

                if (!world.hasVoxel(worldPos)) {
                    return false;
                }

                world.removeVoxel(worldPos);

                return true;
            });
        }
    } else {
        leftMousePressed = false;
    }

    if (input.isMouseRightButtonPressed()) {
        if (!rightMousePressed) {
            rightMousePressed = true;
            castRay(cameraPos, cameraDir, reach, [&](const auto &pos, const auto &prevPos) {
                const auto worldPos = WorldCoordinate(pos);
                const auto prevWorldPos = WorldCoordinate(prevPos);

                if (!world.hasVoxel(worldPos)) {
                    return false;
                }

                world.setVoxel(prevWorldPos, VoxelData(200, 30, 40));
                return true;
            });
        }
    } else {
        rightMousePressed = false;
    }
}
