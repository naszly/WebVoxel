#pragma once

#include <vector>
#include <memory>

#include "Window.h"
#include "System.h"

class Layer {
protected:
    std::vector<std::shared_ptr<System>> m_systems;
public:
    Layer() = default;

    Layer(const Layer&) = delete;
    Layer(Layer&&) = delete;
    Layer& operator=(const Layer&) = delete;
    Layer& operator=(Layer&&) = delete;

    void initialize() const {
        for (const auto& system : m_systems) {
            system->initialize();
        }
    }

    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) const {
        for (const auto& system : m_systems) {
            system->render(encoder, targetView);
        }
    }

    void update(const float dt) const {
        for (const auto& system : m_systems) {
            system->update(dt);
        }
    }

    void onEvent(Event& event) const {
        for (const auto& system : m_systems) {
            system->onEvent(event);
        }
    }

    void pushSystem(std::shared_ptr<System> system) {
        m_systems.push_back(std::move(system));
    }

    template<typename T>
    std::shared_ptr<T> getSystem() {
        for (const auto& system : m_systems) {
            if (auto ptr = std::dynamic_pointer_cast<T>(system)) {
                return ptr;
            }
        }
        return nullptr;
    }
};
