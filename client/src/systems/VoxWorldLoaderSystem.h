#pragma once

#include "core/System.h"

class VoxWorldLoaderSystem final : public System {
public:
    void initialize() override;

    void render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) override;

    void update(float dt) override;

    void onEvent(Event &event) override;

    explicit VoxWorldLoaderSystem(const int modelIndex) : System(), m_modelIndex(modelIndex) { }

private:
    int m_modelIndex;
};
