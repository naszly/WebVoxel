#pragma once

#include <magic_enum.hpp>

enum class EventType {
    None = 0,
    WindowClosed, WindowResized,
    KeyPressed, KeyReleased, KeyTyped,
    MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
};

enum class EventCategory {
    None = 0,
    Application,
    Keyboard,
    Mouse
};

class EventDispatcher;

class Event {
public:
    friend class EventDispatcher;

    virtual ~Event() = default;

    bool handled = false;

    [[nodiscard]] EventType getEventType() const { return type; }

    [[nodiscard]] bool isInCategory(const EventCategory category) const {
        return getCategory() == category;
    }

    [[nodiscard]] EventCategory getCategory() const {
        switch (type) {
            case EventType::None:
                return EventCategory::None;
            case EventType::WindowClosed:
            case EventType::WindowResized:
                return EventCategory::Application;
            case EventType::KeyPressed:
            case EventType::KeyReleased:
            case EventType::KeyTyped:
                return EventCategory::Keyboard;
            case EventType::MouseButtonPressed:
            case EventType::MouseButtonReleased:
            case EventType::MouseMoved:
            case EventType::MouseScrolled:
                return EventCategory::Mouse;
            default:
                return EventCategory::None;
        }
    }



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