#pragma once

#include <vector>
#include <memory>

#include "../Window.h"
#include "../systems/System.h"

class Layer {
protected:
    std::vector<std::shared_ptr<System>> m_Systems;
public:
    Layer() = default;

    Layer(const Layer&) = delete;
    Layer(Layer&&) = delete;
    Layer& operator=(const Layer&) = delete;
    Layer& operator=(Layer&&) = delete;

    void initialize() {
        for (const auto& system : m_Systems) {
            system->initialize();
        }
    }

    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) {
        for (const auto& system : m_Systems) {
            system->render(encoder, targetView);
        }
    }

    void update(const float dt) {
        for (const auto& system : m_Systems) {
            system->update(dt);
        }
    }

    void onEvent(Event& event) {
        for (const auto& system : m_Systems) {
            system->onEvent(event);
        }
    }

    void pushSystem(std::shared_ptr<System> system) {
        m_Systems.push_back(std::move(system));
    }

};
