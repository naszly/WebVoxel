#pragma once

#include <string>
#include <vector>

namespace FileSystem {

    bool FileExists(const std::string& fileName);

    void WriteFile(const std::string& fileName, const char* buffer, size_t size);

    std::vector<char> ReadFile(const std::string& fileName);
};