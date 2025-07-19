#pragma once

#include <string>
#include <vector>

namespace FileSystem {

    bool FileExists(const std::string& fileName);

    void WriteFile(const std::string& fileName, const char* buffer, size_t size);

    /**
     * @brief Reads the contents of a file into a buffer.
     * 
     * When building with Emscripten, this method reads files from the IDBFS virtual filesystem.
     * For native builds, it delegates to ReadFileNative to read files from the local filesystem.
     * 
     * @param fileName The name of the file to read.
     * @return A vector containing the file's contents.
     */
    std::vector<char> ReadFile(const std::string& fileName);

    /**
     * @brief Reads the contents of a file into a buffer.
     * 
     * When building with Emscripten, this method reads files from the preload-file virtual filesystem,
     * which includes files bundled at compile time using Emscripten's `--preload-file` option.
     * For native builds, it reads files directly from the local filesystem.
     * 
     * @param fileName The name of the file to read.
     * @return A vector containing the file's contents.
     */
    std::vector<char> ReadFileNative(const std::string& fileName);

    void CleanFiles(const std::string& extension);

    /**
     * @brief Downloads a file from the filesystem.
     * 
     * In Emscripten builds, this method downloads a file from the virtual filesystem
     * and triggers a browser download. For native builds, this method is not supported.
     * 
     * @param fileName The name of the file to download.
     * @param downloadName The name to use for the downloaded file.
     */
    void Download(const std::string& fileName, const std::string& downloadName);
};
