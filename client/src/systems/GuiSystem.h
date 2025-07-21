#pragma once

#include "core/System.h"

class GuiSystem : public System {
public:
    void initialize() override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    GuiSystem() : System() {}

private:
    static void setImGuiDisplaySize();
    bool m_ambientOcclusion = false;
    bool m_lighting = false;
    bool m_fog = false;
};