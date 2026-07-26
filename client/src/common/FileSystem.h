#pragma once

#include <string>
#include <vector>

namespace FileSystem {

    void initialize();

    bool fileExists(const std::string& fileName);

    void writeFile(const std::string& fileName, const char* buffer, size_t size);
    void ensureDirectory(const std::string& path);
    std::vector<std::string> listDirectories(const std::string& path);

    /**
     * @brief Reads the contents of a file into a buffer.
     * 
     * When building with Emscripten, this method reads files from the IDBFS virtual filesystem.
     * For native builds, it delegates to ReadFileNative to read files from the local filesystem.
     * 
     * @param fileName The name of the file to read.
     * @return A vector containing the file's contents.
     */
    std::vector<char> readFile(const std::string& fileName);

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
    std::vector<char> readFileNative(const std::string& fileName);

    void cleanFiles(const std::string& extension);

    /**
     * @brief Downloads a file from the filesystem.
     * 
     * In Emscripten builds, this method downloads a file from the virtual filesystem
     * and triggers a browser download. For native builds, this method is not supported.
     * 
     * @param fileName The name of the file to download.
     * @param downloadName The name to use for the downloaded file.
     */
    void download(const std::string& fileName, const std::string& downloadName);
};
