#pragma once
#include "System.h"
#include "application/meshing/AmbientOcclusionComputer.h"
#include "application/meshing/AmbientOcclusionComputer.h"
#include "application/meshing/AmbientOcclusionComputer.h"
#include "application/meshing/AmbientOcclusionComputer.h"
#include "application/meshing/AmbientOcclusionComputer.h"
#include "application/meshing/AmbientOcclusionComputer.h"
#include "application/meshing/AmbientOcclusionComputer.h"
#include "application/meshing/AmbientOcclusionComputer.h"
#include "application/meshing/AmbientOcclusionComputer.h"
#include "application/meshing/AmbientOcclusionComputer.h"

class ControllerSystem final : public System {
public:
    void initialize() override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    ControllerSystem() : System() {}

private:
    bool m_isMouseCaptured = false;
    float m_verticalVelocity = 0.0f;
    bool m_onGround = false;
    bool m_gravityEnabled = true;

    // Key constants
    static constexpr auto KEY_FORWARD = KeyCode::W;
    static constexpr auto KEY_BACKWARD = KeyCode::S;
    static constexpr auto KEY_LEFT = KeyCode::A;
    static constexpr auto KEY_RIGHT = KeyCode::D;
    static constexpr auto KEY_SPRINT = KeyCode::LeftShift;
    static constexpr auto KEY_JUMP = KeyCode::Space;
    static constexpr auto KEY_DOWN = KeyCode::C;
    static constexpr auto KEY_ESCAPE = KeyCode::Escape;
    static constexpr auto KEY_MENU = KeyCode::E;
    static constexpr auto KEY_TOGGLE_GRAVITY = KeyCode::G;

    // Camera physics constants
    static constexpr auto CAMERA_COLLISION_EXTENTS = glm::vec3(0.75f, 1.8f, 0.75f);
    static constexpr auto CAMERA_COLLISION_HALF_EXTENTS = CAMERA_COLLISION_EXTENTS * 0.5f;
    static constexpr float CAMERA_EYE_OFFSET_Y = -CAMERA_COLLISION_HALF_EXTENTS.y + 1.5f;
    static constexpr float CAMERA_GRAVITY = -60.0f;
    static constexpr float CAMERA_JUMP_SPEED = 12.0f;
    static constexpr float CAMERA_BASE_SPEED = 9.0f;
    static constexpr float CAMERA_GOD_MODE_SPEED = 20.0f;
    static constexpr float CAMERA_SPRINT_MULTIPLIER = 1.5f;
    static constexpr float CAMERA_FOV_SPRINT_MULTIPLIER = 1.13f;
    static constexpr float CAMERA_FOV_LERP_SPEED = 8.0f;
    static constexpr float CAMERA_COLLISION_PUSH = 0.001f;

    struct SweptAABBResult {
        double entryTime;
        glm::dvec3 normal;
        bool collided;
    };

    using RayHitCallbackFn = std::function<bool(glm::i64vec3, glm::i64vec3)>;

    void updateCameraMovement(float dt, const Input &input, Camera &camera);
    glm::vec3 computeMovementDirection(const Input& input, const Camera& camera) const;
    float computeCameraSpeed(const Input& input) const;
    void moveAndCollideCamera(Camera& camera, glm::dvec3 velocity);
    static SweptAABBResult sweptAABB(const glm::dvec3& aPos, const glm::dvec3& aHalf,
                                     const glm::dvec3& vel,
                                     const glm::dvec3& bPos, const glm::dvec3& bHalf);

    static void animateCameraFov(float dt, const Input& input, Camera& camera);

    static void castRay(glm::vec3 position, glm::vec3 direction, float length, const RayHitCallbackFn &callback);

    // Auto-save timer for player state
    float m_saveTimer = 0.0f;
    static constexpr float SAVE_INTERVAL = 5.0f; // seconds
};
