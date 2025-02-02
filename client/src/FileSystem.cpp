#include "FileSytem.h"

#include <fstream>
#include "Log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

bool FileExists_Emscripten(const char* fileName) {
    bool result = MAIN_THREAD_EM_ASM_INT({
        let fileName = UTF8ToString($0);
        try {
            FS.stat(fileName);
            return 1; // File exists
        } catch (e) {
            return 0; // File does not exist
        }
    }, fileName);

    return result;
}

void ReadFile_Emscripten(const char* fileName, std::vector<char>* buffer) {
    int size = MAIN_THREAD_EM_ASM_INT({
        let size = FS.stat(UTF8ToString($0)).size;
        return size;
    }, fileName);

    buffer->resize(size);

    MAIN_THREAD_EM_ASM({
        let fileName = UTF8ToString($0);
        let size = $1;
        let file = FS.open(fileName, "r");
        let buffer = new Uint8Array(size);
        FS.read(file, buffer, 0, size, 0);
        FS.close(file);
        HEAPU8.set(buffer, $2);
    }, fileName, size, buffer->data());
}

void WriteFile_Emscripten(const char* fileName, const char* buffer, int size) {
    MAIN_THREAD_EM_ASM({
        let fileName = UTF8ToString($0);
        let data = new Uint8Array(HEAPU8.buffer, $1, $2);
        let file = FS.open(fileName, "w+");
        FS.write(file, data, 0, data.length, 0);
        FS.close(file);
    }, fileName, buffer, size);
}

#endif

bool FileSystem::FileExists(const std::string& fileName) {
#if defined(__EMSCRIPTEN__)
    const auto filePath = "/workdir/" + fileName;
    bool result = FileExists_Emscripten(filePath.c_str());
    return result;
#else
    const std::ifstream file(fileName);
    return file.good();
#endif
}

std::vector<char> FileSystem::ReadFile(const std::string& fileName) {
#if defined(__EMSCRIPTEN__)
    const auto filePath = "/workdir/" + fileName;
    std::vector<char> buffer;
    ReadFile_Emscripten(filePath.c_str(), &buffer);
    return buffer;
#else
    std::ifstream file(fileName, std::ios::binary);
    if (!file) {
        LogCore::error("Failed to open file for reading: {0}", fileName);
        return {};
    }

    file.seekg(0, std::ios::end);
    const size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    file.close();

    return buffer;
#endif
}

void FileSystem::WriteFile(const std::string& fileName, const char* buffer, const size_t size) {
#if defined(__EMSCRIPTEN__)
    const auto filePath = "/workdir/" + fileName;
    WriteFile_Emscripten(filePath.c_str(), buffer, size);
#else
    std::ofstream file(fileName, std::ios::binary);
    if (!file) {
        LogCore::error("Failed to open file for writing: {0}", fileName);
        return;
    }

    file.write(buffer, size);
    file.close();
#endif
}