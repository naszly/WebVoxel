#pragma once

#include "Event.h"
#include "MouseCode.h"

class MouseButtonEvent : public Event {
public:
    [[nodiscard]] MouseCode getMouseButton() const { return button; }

protected:
    MouseButtonEvent(const EventType type, const MouseCode button) :
            Event(type), button(button) {}

    MouseCode button;
};

class MouseButtonPressedEvent final : public MouseButtonEvent {
public:
    explicit MouseButtonPressedEvent(const MouseCode button) :
            MouseButtonEvent(eventType, button) {}

    static constexpr EventType eventType = EventType::MouseButtonPressed;

    [[nodiscard]] std::string toString() const override {
        return "MouseButtonPressedEvent: " + std::string(magic_enum::enum_name(button));
    }
};

class MouseButtonReleasedEvent final : public MouseButtonEvent {
public:
    explicit MouseButtonReleasedEvent(const MouseCode button) :
            MouseButtonEvent(eventType, button) {}

    static constexpr EventType eventType = EventType::MouseButtonReleased;

    [[nodiscard]] std::string toString() const override {
        return "MouseButtonReleasedEvent: " + std::string(magic_enum::enum_name(button));
    }
};

class MouseMovedEvent final : public Event {
public:
    MouseMovedEvent(const float x, const float y) :
            Event(eventType), mouseX(x), mouseY(y) {}

    [[nodiscard]] float getX() const { return mouseX; }

    [[nodiscard]] float getY() const { return mouseY; }

    static constexpr EventType eventType = EventType::MouseMoved;

    [[nodiscard]] std::string toString() const override {
        return "MouseMovedEvent: " + std::to_string(mouseX) + "x" + std::to_string(mouseY);
    }

private:
    float mouseX, mouseY;
};

class MouseScrolledEvent final : public Event {
public:
    MouseScrolledEvent(const float xOffset, const float yOffset) :
            Event(eventType), xOffset(xOffset), yOffset(yOffset) {}

    [[nodiscard]] float getXOffset() const { return xOffset; }

    [[nodiscard]] float getYOffset() const { return yOffset; }

    static constexpr EventType eventType = EventType::MouseScrolled;

    [[nodiscard]] std::string toString() const override {
        return "MouseScrolledEvent: " + std::to_string(xOffset) + " " + std::to_string(yOffset);
    }

private:
    float xOffset, yOffset;
};