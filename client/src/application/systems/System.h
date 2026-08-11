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

    
    [[nodiscard]] Application& getApplication() const { return *m_app; }
    [[nodiscard]] ApplicationData& getApplicationData() const;
    
protected:
    [[nodiscard]] Camera& getCamera() const;
    [[nodiscard]] const Input& getInput() const;
    [[nodiscard]] World& getWorld() const;
    [[nodiscard]] const WebGpuContext& getWebGpuContext() const;
    [[nodiscard]] const WebGpuSurface& getWebGpuSurface() const;
    [[nodiscard]] GLFWwindow* getGlfwWindow() const;
    [[nodiscard]] BlockTextureManager& getBlockTextureManager() const;
private:
    Application* m_app = nullptr;
};
