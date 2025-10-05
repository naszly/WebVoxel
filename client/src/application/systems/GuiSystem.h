#pragma once

#include "System.h"

class GuiSystem final : public System {
public:
    void initialize() override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    GuiSystem() : System() {}

private:
    void setImGuiDisplaySize() const;
    bool m_lighting = true;
    bool m_fog = true;
    bool m_pointLight = true;
};