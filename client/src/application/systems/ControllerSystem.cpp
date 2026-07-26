#include "ControllerSystem.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

#include "application/Application.h"
#include "core/events/KeyEvent.h"
#include "core/events/MouseEvent.h"
#include "application/world/World.h"
#include "application/world/WorldCoordinate.h"
#include "application/systems/ChunkManagementSystem.h"
#include "common/Log.h"
#include <limits>

void ControllerSystem::initialize() {
    Camera& camera = getCamera();

    camera.setDirection({0,0,1});
    camera.setPosition({0,150,0});

    // Try to restore saved player state if available
    if (auto* cms = getApplication().getSystem<ChunkManagementSystem>()) {
        if (cms->loadPlayerState(camera)) {
            LogApp::info("Loaded player state from save");
        }
    }
}

void ControllerSystem::render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) {

}

void ControllerSystem::update(const float dt) {
    // Periodically save player state
    m_saveTimer += dt;
    if (m_saveTimer >= SAVE_INTERVAL) {
        if (auto* cms = getApplication().getSystem<ChunkManagementSystem>()) {
            cms->savePlayerState(getCamera());
        }
        m_saveTimer = 0.0f;
    }

    if (!m_isMouseCaptured) return;

    const Input& input = getInput();
    Camera& camera = getCamera();

    updateCameraMovement(dt, input, camera);
    animateCameraFov(dt, input, camera);
}

void ControllerSystem::onEvent(Event &event) {
    Camera& camera = getCamera();
    World& world = getWorld();
    const Input& input = getInput();
    const ApplicationData& appData = getApplicationData();

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
        if ((keyCode == KEY_ESCAPE || keyCode == KEY_MENU) && m_isMouseCaptured) {
            m_isMouseCaptured = false;
            input.setCursorMode(Normal);
            return true;
        }

        if (keyCode == KEY_TOGGLE_GRAVITY) {
            m_gravityEnabled = !m_gravityEnabled;
            return true;
        }

        return false;
    });
}

void ControllerSystem::updateCameraMovement(const float dt, const Input &input, Camera &camera) {

    if (m_gravityEnabled) {
        if (input.isKeyPressed(KEY_JUMP) && m_onGround) {
            m_verticalVelocity = CAMERA_JUMP_SPEED;
            m_onGround = false;
        }
    } else {
        m_verticalVelocity = 0.0f;
        m_onGround = false;
    }

    const glm::vec3 moveDirection = computeMovementDirection(input, camera);
    const float speed = computeCameraSpeed(input);

    glm::vec3 velocity = moveDirection * speed;
    velocity.y += m_verticalVelocity;
    velocity *= dt;

    moveAndCollideCamera(camera, velocity);

    m_verticalVelocity += CAMERA_GRAVITY * dt;
}

glm::vec3 ControllerSystem::computeMovementDirection(const Input& input, const Camera& camera) const {
    const auto cameraDirection = camera.getDirection();
    const glm::vec3 flatDirection = glm::normalize(glm::vec3(cameraDirection.x, 0.0f, cameraDirection.z));
    const glm::vec3 cameraRight = glm::normalize(glm::vec3(flatDirection.z, 0.0f, -flatDirection.x));

    glm::vec3 moveDir(0.0f);
    if (m_gravityEnabled) {
        if (input.isKeyPressed(KEY_FORWARD)) moveDir += flatDirection;
        if (input.isKeyPressed(KEY_BACKWARD)) moveDir -= flatDirection;
        if (input.isKeyPressed(KEY_LEFT)) moveDir -= cameraRight;
        if (input.isKeyPressed(KEY_RIGHT)) moveDir += cameraRight;
    } else {
        if (input.isKeyPressed(KEY_FORWARD)) moveDir += cameraDirection;
        if (input.isKeyPressed(KEY_BACKWARD)) moveDir -= cameraDirection;
        if (input.isKeyPressed(KEY_LEFT)) moveDir -= cameraRight;
        if (input.isKeyPressed(KEY_RIGHT)) moveDir += cameraRight;
        if (input.isKeyPressed(KEY_JUMP)) moveDir.y += 1.0f;
        if (input.isKeyPressed(KEY_DOWN)) moveDir.y -= 1.0f;
    }

    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir);
    }
    return moveDir;
}

float ControllerSystem::computeCameraSpeed(const Input& input) const {
    float speed = m_gravityEnabled ? CAMERA_BASE_SPEED : CAMERA_GOD_MODE_SPEED;
    if (input.isKeyPressed(KEY_SPRINT)) {
        speed *= CAMERA_SPRINT_MULTIPLIER;
    }
    return speed;
}

void ControllerSystem::moveAndCollideCamera(Camera& camera, glm::dvec3 velocity) {
    constexpr glm::dvec3 boxHalfExtents = CAMERA_COLLISION_HALF_EXTENTS;
    const World& world = getWorld();
    double epsilon = 1e-10; // small epsilon to avoid precision issues
    glm::dvec3 position = glm::dvec3(camera.getPosition()) - glm::dvec3(0, CAMERA_EYE_OFFSET_Y - epsilon, 0);

    for (int sweep = 0; sweep < 3; ++sweep) {
        double earliestHit = 1.0;
        glm::dvec3 hitNormal(0.0);
        glm::dvec3 sweepEnd = position + velocity;
        glm::dvec3 minSweep = glm::min(position - boxHalfExtents, sweepEnd - boxHalfExtents);
        glm::dvec3 maxSweep = glm::max(position + boxHalfExtents, sweepEnd + boxHalfExtents);
        for (int x = static_cast<int>(std::floor(minSweep.x)); x <= static_cast<int>(std::floor(maxSweep.x)); ++x) {
            for (int y = static_cast<int>(std::floor(minSweep.y)); y <= static_cast<int>(std::floor(maxSweep.y)); ++y) {
                for (int z = static_cast<int>(std::floor(minSweep.z)); z <= static_cast<int>(std::floor(maxSweep.z)); ++z) {
                    WorldCoordinate coord(glm::i64vec3(x, y, z));
                    if (!world.hasVoxel(coord)) continue;
                    glm::dvec3 voxelPos(x + 0.5, y + 0.5, z + 0.5);
                    glm::dvec3 voxelHalf(0.5);
                    SweptAABBResult res = sweptAABB(position, boxHalfExtents, velocity, voxelPos, voxelHalf);
                    if (res.collided && res.entryTime < earliestHit) {
                        earliestHit = res.entryTime;
                        hitNormal = res.normal;
                    }
                }
            }
        }
        if (earliestHit < 1.0) {
            position += velocity * earliestHit;
            position += hitNormal * static_cast<double>(CAMERA_COLLISION_PUSH);
            if (hitNormal.y > 0.0) {
                m_verticalVelocity = 0.0f;
                m_onGround = true;
            }
            velocity -= hitNormal * glm::dot(velocity, hitNormal);
        } else {
            position += velocity;
            break;
        }
    }

    camera.setPosition(position + glm::dvec3(0, CAMERA_EYE_OFFSET_Y, 0)); // set eye above center
}

void ControllerSystem::animateCameraFov(const float dt, const Input& input, Camera& camera) {
    static float animatedFov = Camera::DEFAULT_FOV;
    const bool sprinting = input.isKeyPressed(KEY_FORWARD) && input.isKeyPressed(KEY_SPRINT);
    const float targetFov = sprinting ? Camera::DEFAULT_FOV * CAMERA_FOV_SPRINT_MULTIPLIER : Camera::DEFAULT_FOV;
    animatedFov += (targetFov - animatedFov) * glm::clamp(CAMERA_FOV_LERP_SPEED * dt, 0.0f, 1.0f);
    camera.setFov(animatedFov);
}

ControllerSystem::SweptAABBResult ControllerSystem::sweptAABB(const glm::dvec3& aPos, const glm::dvec3& aHalf,
                                                              const glm::dvec3& vel,
                                                              const glm::dvec3& bPos, const glm::dvec3& bHalf) {
    glm::dvec3 invEntry, invExit;
    for (int i = 0; i < 3; ++i) {
        if (vel[i] > 0.0) {
            invEntry[i] = (bPos[i] - bHalf[i]) - (aPos[i] + aHalf[i]);
            invExit[i]  = (bPos[i] + bHalf[i]) - (aPos[i] - aHalf[i]);
        } else if (vel[i] < 0.0) {
            invEntry[i] = (bPos[i] + bHalf[i]) - (aPos[i] - aHalf[i]);
            invExit[i]  = (bPos[i] - bHalf[i]) - (aPos[i] + aHalf[i]);
        } else {
            invEntry[i] = -std::numeric_limits<double>::infinity();
            invExit[i]  =  std::numeric_limits<double>::infinity();
        }
    }
    glm::dvec3 entry, exit;
    for (int i = 0; i < 3; ++i) {
        if (vel[i] == 0.0) {
            entry[i] = -std::numeric_limits<double>::infinity();
            exit[i]  =  std::numeric_limits<double>::infinity();
        } else {
            entry[i] = invEntry[i] / vel[i];
            exit[i]  = invExit[i] / vel[i];
        }
    }
    const double entryTime = std::max({ entry.x, entry.y, entry.z });
    const double exitTime  = std::min({ exit.x, exit.y, exit.z });
    if (entryTime > exitTime || (entry.x < 0 && entry.y < 0 && entry.z < 0) || entryTime > 1.0 || entryTime < 0.0) {
        return {1.0, glm::dvec3(0.0), false};
    }
    glm::dvec3 normal(0.0);
    if (entryTime == entry.x) normal.x = vel.x > 0 ? -1.0 : 1.0;
    else if (entryTime == entry.y) normal.y = vel.y > 0 ? -1.0 : 1.0;
    else if (entryTime == entry.z) normal.z = vel.z > 0 ? -1.0 : 1.0;
    return {entryTime, normal, true};
}

void ControllerSystem::castRay(glm::vec3 position, glm::vec3 direction, float length, const RayHitCallbackFn &callback) {
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
