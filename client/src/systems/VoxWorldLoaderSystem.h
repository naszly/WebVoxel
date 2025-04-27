#pragma once

#include "System.h"
#include "../Thread.h"

class VoxWorldLoaderSystem final : public System {
public:
    void initialize() override;

    void render(const WGPUCommandEncoder &encoder, const WGPUTextureView &targetView) override;

    void update(float dt) override;

    void onEvent(Event &event) override;

private:
    std::unique_ptr<Threading::Worker> m_LoadWorker;
};
