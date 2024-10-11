#pragma once

#include "../Window.h"
#include "../Camera.h"
#include "../world/World.h"

class System {
public:
    System(const System&) = delete;
    System(System&&) = delete;
    System& operator=(const System&) = delete;
    System& operator=(System&&) = delete;

    System() = default;

    virtual ~System() = default;
    virtual void initialize(const Window&) = 0;
    virtual void render() = 0;
    virtual void update(float dt) = 0;
    virtual void onEvent(Event& event) = 0;

protected:
    static Camera& GetCamera();
    static const Input& GetInput();
    static const World& GetWorld();
};