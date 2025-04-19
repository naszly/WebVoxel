
#include "Timer.h"

#include "Log.h"
#include "FileSytem.h"

#include <unordered_map>
#include <string>
#include <sstream>

std::unordered_map<std::string, std::vector<double>> savedTimes;

Timer::Timer(const char *name): m_Name(name),  m_Start(std::chrono::high_resolution_clock::now()) {}

Timer::~Timer() {
    const double ms = elapsedMilliseconds();
    LogApp::info("{}: {} ms", m_Name, ms);
    savedTimes[m_Name].push_back(ms);
}

void Timer::exportTimes() {
    for (auto&[name, timeValues] : savedTimes) {
        std::string fileName = name + ".txt";

        std::vector<std::string> lines;
        for (const auto& time : timeValues) {
            lines.push_back(std::to_string(time));
        }

        std::stringstream ss;
        for (const auto& line : lines) {
            ss << line << "\n";
        }

        FileSystem::WriteFile(fileName, ss.str().c_str(), ss.str().size());
    }
}

double Timer::elapsedMilliseconds() const {
    const auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - m_Start).count();
}
