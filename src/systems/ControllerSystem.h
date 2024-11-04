#pragma once
#include "System.h"

class ControllerSystem final : public System {
public:
    void initialize() override;
    void render() override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    ControllerSystem() : System() {}
};
