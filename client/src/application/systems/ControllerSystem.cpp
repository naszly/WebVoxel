#include "ControllerSystem.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

#include "application/Application.h"
#include "core/events/KeyEvent.h"
#include "core/events/MouseEvent.h"
#include "application/world/World.h"
#include "application/world/WorldCoordinate.h"
#include <limits>

using RayHitCallbackFn = std::function<bool(glm::i64vec3, glm::i64vec3)>;
void castRay(glm::vec3 position, glm::vec3 direction, float length, const RayHitCallbackFn &callback);

struct SweptAABBResult {
    float entryTime;
    glm::vec3 normal;
    bool collided;
};

// Swept AABB: returns collision time, normal, and collision flag
static SweptAABBResult sweptAABB(const glm::vec3& aPos, const glm::vec3& aHalf,
                                 const glm::vec3& vel,
                                 const glm::vec3& bPos, const glm::vec3& bHalf) {
    glm::vec3 invEntry, invExit;
    for (int i = 0; i < 3; ++i) {
        if (vel[i] > 0.0f) {
            invEntry[i] = (bPos[i] - bHalf[i]) - (aPos[i] + aHalf[i]);
            invExit[i]  = (bPos[i] + bHalf[i]) - (aPos[i] - aHalf[i]);
        } else if (vel[i] < 0.0f) {
            invEntry[i] = (bPos[i] + bHalf[i]) - (aPos[i] - aHalf[i]);
            invExit[i]  = (bPos[i] - bHalf[i]) - (aPos[i] + aHalf[i]);
        } else {
            invEntry[i] = -std::numeric_limits<float>::infinity();
            invExit[i]  = std::numeric_limits<float>::infinity();
        }
    }
    glm::vec3 entry, exit;
    for (int i = 0; i < 3; ++i) {
        if (vel[i] == 0.0f) {
            entry[i] = -std::numeric_limits<float>::infinity();
            exit[i]  = std::numeric_limits<float>::infinity();
        } else {
            entry[i] = invEntry[i] / vel[i];
            exit[i]  = invExit[i] / vel[i];
        }
    }
    float entryTime = std::max({ entry.x, entry.y, entry.z });
    float exitTime  = std::min({ exit.x, exit.y, exit.z });
    if (entryTime > exitTime || (entry.x < 0 && entry.y < 0 && entry.z < 0) || entryTime > 1.0f || entryTime < 0.0f) {
        return {1.0f, glm::vec3(0.0f), false};
    }
    glm::vec3 normal(0.0f);
    if (entryTime == entry.x) normal.x = vel.x > 0 ? -1.0f : 1.0f;
    else if (entryTime == entry.y) normal.y = vel.y > 0 ? -1.0f : 1.0f;
    else if (entryTime == entry.z) normal.z = vel.z > 0 ? -1.0f : 1.0f;
    return {entryTime, normal, true};
}

void ControllerSystem::initialize() {
    Camera& camera = getCamera();

    camera.setDirection({0,0,1});
    camera.setPosition({0,150,0});
}

void ControllerSystem::render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) {

}

void ControllerSystem::update(const float dt) {
    const Input& input = getInput();
    Camera& camera = getCamera();

    if (m_isMouseCaptured) {
        updateCamera(dt, input, camera);
    }
}

void ControllerSystem::onEvent(Event &event) {
    Camera& camera = getCamera();
    World& world = getWorld();
    const Input& input = getInput();
    const ApplicationData &appData = Application::getInstance().getApplicationData();

    EventDispatcher dispatcher(event);

    dispatcher.dispatch<MouseMovedEvent>([&](const MouseMovedEvent &mouseEvent) {
        const auto xPos = mouseEvent.getX();
        const auto yPos = mouseEvent.getY();

        static double lastX = xPos;
        static double lastY = yPos;
        static bool firstMouse = true;

#ifdef __EMSCRIPTEN__
        {
            EmscriptenPointerlockChangeEvent pointerlockStatus;
            emscripten_get_pointerlock_status(&pointerlockStatus);
            if (m_isMouseCaptured != pointerlockStatus.isActive) {
                m_isMouseCaptured = pointerlockStatus.isActive;
                input.setCursorMode(m_isMouseCaptured ? Disabled : Normal);
            }
        }
#endif

        if (!m_isMouseCaptured) {
            firstMouse = true;
            return false;
        }

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

    dispatcher.dispatch<MouseButtonPressedEvent>([&](const MouseButtonPressedEvent &mouseEvent) {
        const auto button = mouseEvent.getMouseButton();
        const auto cameraPos = camera.getPosition();
        const auto cameraDir = glm::normalize(camera.getDirection());
        constexpr float reach = 1024.0f;

        if (button == MouseCode::ButtonLeft && !m_isMouseCaptured) {
            m_isMouseCaptured = true;
            input.setCursorMode(Disabled);
            return true;
        }

        if (button == MouseCode::ButtonLeft && m_isMouseCaptured) {
            castRay(cameraPos, cameraDir, reach, [&](const auto &pos, const auto &prevPos) {
                const auto worldPos = WorldCoordinate(pos);

                if (!world.hasVoxel(worldPos)) {
                    return false;
                }

                world.removeVoxel(worldPos, appData.placedVoxelRadius, appData.placedVoxelShapeIsSphere);

                return true;
            });

            return true;
        }

        if (button == MouseCode::ButtonRight && m_isMouseCaptured) {
            castRay(cameraPos, cameraDir, reach, [&](const auto &pos, const auto &prevPos) {
                const auto worldPos = WorldCoordinate(pos);
                const auto prevWorldPos = WorldCoordinate(prevPos);

                if (!world.hasVoxel(worldPos)) {
                    return false;
                }

                world.setVoxel(prevWorldPos,
                    appData.selectedVoxel,
                    appData.placedVoxelRadius,
                    appData.placedVoxelShapeIsSphere);

                return true;
            });

            return true;
        }

        return false;
    });

    dispatcher.dispatch<KeyPressedEvent>([&](const KeyPressedEvent &keyEvent) {
        const auto keyCode = keyEvent.getKeyCode();
        if ((keyCode == KeyCode::Escape || keyCode == KeyCode::E) && m_isMouseCaptured) {
            m_isMouseCaptured = false;
            input.setCursorMode(Normal);
            return true;
        }

        return false;
    });
}

void ControllerSystem::updateCamera(const float dt, const Input &input, Camera &camera) {
    const auto cameraDirection = camera.getDirection();
    glm::vec3 flatDirection = glm::normalize(glm::vec3(cameraDirection.x, 0.0f, cameraDirection.z));
    glm::vec3 cameraRight = glm::normalize(glm::vec3(flatDirection.z, 0.0f, -flatDirection.x));
    constexpr glm::vec3 boxHalfExtents(0.4925f, 1.55f, 0.4925f);
    constexpr float gravity = -60.0f;
    constexpr float jumpSpeed = 12.0f;
    static float verticalVelocity = 0.0f;
    static bool onGround = false;
    float speed = 9.0f;

    glm::vec3 moveDir(0.0f);
    if (input.isKeyPressed(KeyCode::W)) moveDir += flatDirection;
    if (input.isKeyPressed(KeyCode::S)) moveDir -= flatDirection;
    if (input.isKeyPressed(KeyCode::A)) moveDir -= cameraRight;
    if (input.isKeyPressed(KeyCode::D)) moveDir += cameraRight;
    if (input.isKeyPressed(KeyCode::LeftShift)) {
        speed *= 1.5;
    }
    if (input.isKeyPressed(KeyCode::Space) && onGround) {
        verticalVelocity = jumpSpeed;
        onGround = false;
    }

    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir);
    }

    glm::vec3 velocity = moveDir * speed;
    velocity.y += verticalVelocity;
    glm::vec3 position = camera.getPosition();
    World& world = getWorld();

    velocity *= dt;

    for (int sweep = 0; sweep < 3; ++sweep) {
        float earliestHit = 1.0f;
        glm::vec3 hitNormal(0.0f);
        glm::vec3 sweepEnd = position + velocity;
        glm::vec3 minSweep = glm::min(position - boxHalfExtents, sweepEnd - boxHalfExtents);
        glm::vec3 maxSweep = glm::max(position + boxHalfExtents, sweepEnd + boxHalfExtents);
        for (int x = static_cast<int>(std::floor(minSweep.x)); x <= static_cast<int>(std::floor(maxSweep.x)); ++x) {
            for (int y = static_cast<int>(std::floor(minSweep.y)); y <= static_cast<int>(std::floor(maxSweep.y)); ++y) {
                for (int z = static_cast<int>(std::floor(minSweep.z)); z <= static_cast<int>(std::floor(maxSweep.z)); ++z) {
                    WorldCoordinate coord(glm::i64vec3(x, y, z));
                    if (!world.hasVoxel(coord)) continue;
                    glm::vec3 voxelPos(x + 0.5f, y + 0.5f, z + 0.5f);
                    glm::vec3 voxelHalf(0.5f);
                    SweptAABBResult res = sweptAABB(position, boxHalfExtents, velocity, voxelPos, voxelHalf);
                    if (res.collided && res.entryTime < earliestHit) {
                        earliestHit = res.entryTime;
                        hitNormal = res.normal;
                    }
                }
            }
        }
        if (earliestHit < 1.0f) {
            position += velocity * earliestHit;
            position += hitNormal * 0.001f;
            if (hitNormal.y > 0.0f) {
                verticalVelocity = 0.0f;
                onGround = true;
            }
            velocity -= hitNormal * glm::dot(velocity, hitNormal);
        } else {
            position += velocity;
            break;
        }
    }

    verticalVelocity += gravity * dt;
    camera.setPosition(position);
}

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

    while (!callback(current, previous) && current != end) {
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
