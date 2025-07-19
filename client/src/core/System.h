#pragma once

#include "Window.h"
#include "Camera.h"
#include "world/World.h"

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

protected:
    static Camera& GetCamera();
    static const Input& GetInput();
    static World& GetWorld();
    static const WebGPUContext& GetWebGPUContext();
    static const WebGPUSurface& GetWebGPUSurface();
    static GLFWwindow* GetGLFWWindow();
};