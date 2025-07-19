#pragma once

#include "Event.h"
#include "core/MouseCode.h"

class MouseButtonEvent : public Event {
public:
    [[nodiscard]] MouseCode getMouseButton() const { return m_button; }

protected:
    MouseButtonEvent(const EventType type, const MouseCode button) :
            Event(type), m_button(button) {}

    MouseCode m_button;
};

class MouseButtonPressedEvent final : public MouseButtonEvent {
public:
    explicit MouseButtonPressedEvent(const MouseCode button) :
            MouseButtonEvent(EVENT_TYPE, button) {}

    static constexpr EventType EVENT_TYPE = EventType::MouseButtonPressed;

    [[nodiscard]] std::string toString() const override {
        return "MouseButtonPressedEvent: " + std::string(magic_enum::enum_name(m_button));
    }
};

class MouseButtonReleasedEvent final : public MouseButtonEvent {
public:
    explicit MouseButtonReleasedEvent(const MouseCode button) :
            MouseButtonEvent(EVENT_TYPE, button) {}

    static constexpr EventType EVENT_TYPE = EventType::MouseButtonReleased;

    [[nodiscard]] std::string toString() const override {
        return "MouseButtonReleasedEvent: " + std::string(magic_enum::enum_name(m_button));
    }
};

class MouseMovedEvent final : public Event {
public:
    MouseMovedEvent(const float x, const float y) :
            Event(EVENT_TYPE), m_mouseX(x), m_mouseY(y) {}

    [[nodiscard]] float getX() const { return m_mouseX; }

    [[nodiscard]] float getY() const { return m_mouseY; }

    static constexpr EventType EVENT_TYPE = EventType::MouseMoved;

    [[nodiscard]] std::string toString() const override {
        return "MouseMovedEvent: " + std::to_string(m_mouseX) + "x" + std::to_string(m_mouseY);
    }

private:
    float m_mouseX, m_mouseY;
};

class MouseScrolledEvent final : public Event {
public:
    MouseScrolledEvent(const float xOffset, const float yOffset) :
            Event(EVENT_TYPE), m_xOffset(xOffset), m_yOffset(yOffset) {}

    [[nodiscard]] float getXOffset() const { return m_xOffset; }

    [[nodiscard]] float getYOffset() const { return m_yOffset; }

    static constexpr EventType EVENT_TYPE = EventType::MouseScrolled;

    [[nodiscard]] std::string toString() const override {
        return "MouseScrolledEvent: " + std::to_string(m_xOffset) + " " + std::to_string(m_yOffset);
    }

private:
    float m_xOffset, m_yOffset;
};