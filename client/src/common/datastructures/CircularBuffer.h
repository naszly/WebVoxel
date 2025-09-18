#pragma once

#include <optional>

template <typename T, size_t Capacity>
class CircularBuffer {
public:
    CircularBuffer() = default;
    ~CircularBuffer() = default;

    bool push(const T& value) {
        if (m_size == Capacity) {
            return false;
        }
        m_buffer[(m_head + m_size) % Capacity] = value;
        ++m_size;
        return true;
    }

    bool push(T&& value) {
        if (m_size == Capacity) {
            return false;
        }
        m_buffer[(m_head + m_size) % Capacity] = std::move(value);
        ++m_size;
        return true;
    }

    T& first() {
        return m_buffer[m_head];
    }

    void removeFirst() {
        if (m_size == 0) {
            return;
        }
        m_head = (m_head + 1) % Capacity;
        --m_size;
    }

    T pop() {
        T value = m_buffer[m_head];
        m_head = (m_head + 1) % Capacity;
        --m_size;
        return value;
    }

    std::optional<T> tryPop() {
        if (m_size == 0) {
            return std::nullopt;
        }
        return pop();
    }

    void clear() {
        m_size = 0;
        m_head = 0;
    }

    [[nodiscard]] size_t size() const {
        return m_size;
    }

    [[nodiscard]] bool empty() const {
        return m_size == 0;
    }

    [[nodiscard]] bool full() const {
        return m_size == Capacity;
    }

private:
    T m_buffer[Capacity]{};
    size_t m_size{0};
    size_t m_head{0};
};
