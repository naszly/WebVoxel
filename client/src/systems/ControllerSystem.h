#pragma once
#include "System.h"

class ControllerSystem final : public System {
public:
    void initialize() override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    ControllerSystem() : System() {}

private:
    bool isMouseCaptured = false;
    static void updateCamera(float dt, const Input &input, Camera &camera);
};
