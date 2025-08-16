#pragma once

#include "core/Window.h"
#include "../graphics/Camera.h"
#include "../world/World.h"
#include "application/graphics/BlockTextureManager.h"

struct ApplicationData;
class Application;

class System {
public:
    System(const System&) = delete;
    System(System&&) = delete;
    System& operator=(const System&) = delete;
    System& operator=(System&&) = delete;

    System() = default;

    virtual ~System() = default;
    virtual void initialize() = 0;
    virtual void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) = 0;
    virtual void update(float dt) = 0;
    virtual void onEvent(Event& event) = 0;

    void setAppReference(Application& app) { m_app = &app; }

protected:
    Application& getApplication() const { return *m_app; }
    ApplicationData& getApplicationData() const;
    Camera& getCamera() const;
    const Input& getInput() const;
    World& getWorld() const;
    const WebGpuContext& getWebGpuContext() const;
    const WebGpuSurface& getWebGpuSurface() const;
    GLFWwindow* getGlfwWindow() const;
    BlockTextureManager& getBlockTextureManager() const;
private:
    Application* m_app = nullptr;
};
