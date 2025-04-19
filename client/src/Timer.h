#pragma once

#include <chrono>

class Timer {
public:
    explicit Timer(const char* name);
    ~Timer();

    static void exportTimes();
private:
    const char* m_Name;
    const std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;

    [[nodiscard]] double elapsedMilliseconds() const;
};