#pragma once

#include <vector>
#include <memory>

#include "core/Window.h"
#include "systems/System.h"

class Layer {
protected:
    std::vector<std::unique_ptr<System>> m_systems;
public:
    Layer() = default;
    ~Layer() {
        for (auto& system : m_systems) {
            system.reset();
        }
        m_systems.clear();
    }

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

    template<typename T, typename... Args>
    void pushSystem(Args&&... args) {
        static_assert(std::is_base_of_v<System, T>, "T must inherit from System");
        m_systems.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    }

    void setAppReference(Application& app) const {
        for (auto& system : m_systems) {
            system->setAppReference(app);
        }
    }

    template<typename T>
    T* getSystem() {
        for (const auto& system : m_systems) {
            if (auto ptr = dynamic_cast<T*>(system.get())) {
                return ptr;
            }
        }
        return nullptr;
    }
};
