#pragma once

#include <chrono>

class Timer {
public:
    explicit Timer(const char* name);
    ~Timer();

    static void exportTimes();
private:
    const char* m_name;
    const std::chrono::time_point<std::chrono::high_resolution_clock> m_start;

    [[nodiscard]] double elapsedMilliseconds() const;
};