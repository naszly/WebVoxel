#pragma once

#include "Event.h"
#include "KeyCode.h"

class KeyEvent : public Event {
public:
    [[nodiscard]] KeyCode getKeyCode() const { return keyCode; }

protected:
    KeyEvent(const EventType type, const KeyCode keyCode) :
            Event(type), keyCode(keyCode) {}

    KeyCode keyCode;
};

class KeyPressedEvent final : public KeyEvent {
public:
    explicit KeyPressedEvent(const KeyCode keyCode, const bool repeat = false) :
            KeyEvent(eventType, keyCode), repeat(repeat) {}

    [[nodiscard]] bool isRepeat() const { return repeat; }

    static constexpr EventType eventType = EventType::KeyPressed;

    [[nodiscard]] std::string toString() const override {
        return "KeyPressedEvent: " + std::string(magic_enum::enum_name(keyCode)) + " (" + std::to_string(repeat) + ")";
    }

private:
    bool repeat;
};

class KeyReleasedEvent final : public KeyEvent {
public:
    explicit KeyReleasedEvent(const KeyCode keyCode) :
            KeyEvent(eventType, keyCode) {}

    static constexpr EventType eventType = EventType::KeyReleased;

    [[nodiscard]] std::string toString() const override {
        return "KeyReleasedEvent: " + std::string(magic_enum::enum_name(keyCode));
    }
};

class KeyTypedEvent final : public KeyEvent {
public:
    explicit KeyTypedEvent(const KeyCode keyCode) :
            KeyEvent(eventType, keyCode) {}

    static constexpr EventType eventType = EventType::KeyTyped;

    [[nodiscard]] std::string toString() const override {
        return "KeyTypedEvent: " + std::string(magic_enum::enum_name(keyCode));
    }
};