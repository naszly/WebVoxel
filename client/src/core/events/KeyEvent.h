#pragma once

#include "Event.h"
#include "core/KeyCode.h"

class KeyEvent : public Event {
public:
    [[nodiscard]] KeyCode getKeyCode() const { return m_keyCode; }

protected:
    KeyEvent(const EventType type, const KeyCode keyCode) :
            Event(type), m_keyCode(keyCode) {}

    KeyCode m_keyCode;
};

class KeyPressedEvent final : public KeyEvent {
public:
    explicit KeyPressedEvent(const KeyCode keyCode, const bool repeat = false) :
            KeyEvent(EVENT_TYPE, keyCode), m_repeat(repeat) {}

    [[nodiscard]] bool isRepeat() const { return m_repeat; }

    static constexpr EventType EVENT_TYPE = EventType::KeyPressed;

    [[nodiscard]] std::string toString() const override {
        return "KeyPressedEvent: " + std::string(magic_enum::enum_name(m_keyCode)) + " (" + std::to_string(m_repeat) + ")";
    }

private:
    bool m_repeat;
};

class KeyReleasedEvent final : public KeyEvent {
public:
    explicit KeyReleasedEvent(const KeyCode keyCode) :
            KeyEvent(EVENT_TYPE, keyCode) {}

    static constexpr EventType EVENT_TYPE = EventType::KeyReleased;

    [[nodiscard]] std::string toString() const override {
        return "KeyReleasedEvent: " + std::string(magic_enum::enum_name(m_keyCode));
    }
};

class KeyTypedEvent final : public KeyEvent {
public:
    explicit KeyTypedEvent(const KeyCode keyCode) :
            KeyEvent(EVENT_TYPE, keyCode) {}

    static constexpr EventType EVENT_TYPE = EventType::KeyTyped;

    [[nodiscard]] std::string toString() const override {
        return "KeyTypedEvent: " + std::string(magic_enum::enum_name(m_keyCode));
    }
};