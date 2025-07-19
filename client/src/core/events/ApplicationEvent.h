#pragma once

#include "Event.h"

class WindowResizedEvent final : public Event {
public:
    WindowResizedEvent(const unsigned int width, const unsigned int height) :
            Event(EVENT_TYPE), m_width(width), m_height(height) {}

    [[nodiscard]] unsigned int getWidth() const { return m_width; }

    [[nodiscard]] unsigned int getHeight() const { return m_height; }

    static constexpr EventType EVENT_TYPE = EventType::WindowResized;

    [[nodiscard]] std::string toString() const override {
        return "WindowResizedEvent" + std::to_string(m_width) + "x" + std::to_string(m_height);
    }

private:
    unsigned int m_width, m_height;
};

class WindowClosedEvent final : public Event {
public:
    WindowClosedEvent() : Event(EVENT_TYPE) {}

    static constexpr EventType EVENT_TYPE = EventType::WindowClosed;

    [[nodiscard]] std::string toString() const override {
        return "WindowClosedEvent";
    }
};
