
#include "Timer.h"

#include "Log.h"

Timer::Timer(const char *name): m_Name(name),  m_Start(std::chrono::high_resolution_clock::now()) {}

Timer::~Timer() {
    const double ms = elapsedMilliseconds();
    LogApp::info("{}: {} ms", m_Name, ms);
}

double Timer::elapsedMilliseconds() const {
    const auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - m_Start).count();
}
