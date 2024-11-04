#pragma once

#include "Event.h"

class WindowResizedEvent final : public Event {
public:
    WindowResizedEvent(const unsigned int width, const unsigned int height) :
            Event(eventType), width(width), height(height) {}

    [[nodiscard]] unsigned int getWidth() const { return width; }

    [[nodiscard]] unsigned int getHeight() const { return height; }

    static constexpr EventType eventType = EventType::WindowResized;

    [[nodiscard]] std::string toString() const override {
        return "WindowResizedEvent" + std::to_string(width) + "x" + std::to_string(height);
    }

private:
    unsigned int width, height;
};

class WindowClosedEvent final : public Event {
public:
    WindowClosedEvent() : Event(eventType) {}

    static constexpr EventType eventType = EventType::WindowClosed;

    [[nodiscard]] std::string toString() const override {
        return "WindowClosedEvent";
    }
};
