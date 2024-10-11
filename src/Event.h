#pragma once

#include <magic_enum.hpp>

enum class EventType {
    None = 0,
    WindowClose, WindowResize,
    KeyPressed, KeyReleased, KeyTyped,
    MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
};

class EventDispatcher;

class Event {
public:
    friend class EventDispatcher;

    virtual ~Event() = default;

    bool handled = false;

    [[nodiscard]] EventType getEventType() const { return type; }

    [[nodiscard]] virtual std::string toString() const {
        return "Event: " + std::string(magic_enum::enum_name(type));
    }

protected:
    explicit Event(const EventType type) : type(type) {}

private:
    EventType type;
};

using EventCallbackFn = std::function<void(Event &)>;

class EventDispatcher {
public:
    explicit EventDispatcher(Event &event) : event(event) {}

    // F will be deduced by the compiler
    template<typename T, typename F>
    bool dispatch(const F &func) {
        if (event.getEventType() == T::eventType) {
            event.handled |= func(static_cast<T &>(event));
            return true;
        }
        return false;
    }

private:
    Event &event;
};