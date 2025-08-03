
#include "Timer.h"

#include "datastructures/HashMap.h"
#include "Log.h"
#include "FileSystem.h"

#include <string>
#include <sstream>

HashMap<std::string, std::vector<double>> savedTimes;

Timer::Timer(const char *name): m_name(name),  m_start(std::chrono::high_resolution_clock::now()) {}

Timer::~Timer() {
    const double ms = elapsedMilliseconds();
    LogApp::info("{}: {} ms", m_name, ms);
    savedTimes[m_name].push_back(ms);
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

        FileSystem::writeFile(fileName, ss.str().c_str(), ss.str().size());
    }
}

double Timer::elapsedMilliseconds() const {
    const auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - m_start).count();
}
