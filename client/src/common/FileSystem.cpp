#include "FileSystem.h"

#include "Log.h"

#include <fstream>
#include <filesystem>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

bool syncfsFinished = false;

extern "C" {
    void onSyncfsFinished() {
        syncfsFinished = true;
    }
}

void waitInitialSyncfs() {
    while (!syncfsFinished) {
        emscripten_sleep(100);
    }
}

bool FileExists_Emscripten(const char* fileName) {
    waitInitialSyncfs();

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
    waitInitialSyncfs();

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
    waitInitialSyncfs();

    MAIN_THREAD_EM_ASM({
        let fileName = UTF8ToString($0);
        let data = new Uint8Array(HEAPU8.buffer, $1, $2);
        let file = FS.open(fileName, "w+");
        FS.write(file, data, 0, data.length, 0);
        FS.close(file);
    }, fileName, buffer, size);
}

void DownloadFile_Emscripten(const char* fileName, const char* downloadName) {
    waitInitialSyncfs();

    MAIN_THREAD_EM_ASM({
        let fileName = UTF8ToString($0);
        let downloadName = UTF8ToString($1);

        try {
            let data = FS.readFile(fileName, { encoding: 'binary' });
            let blob = new Blob([new Uint8Array(data)], { type: 'application/octet-stream' });

            let link = document.createElement('a');
            link.href = URL.createObjectURL(blob);
            link.download = downloadName;
            link.click();

            URL.revokeObjectURL(link.href);
        } catch (readErr) {
            console.error('Could not read file:', readErr);
        }
    }, fileName, downloadName);
}
#endif

void FileSystem::initialize() {
#if defined(__EMSCRIPTEN__)
    EM_ASM(
        let pathInfo = FS.analyzePath("/workdir");

        if (!pathInfo.exists) {
            FS.mkdir('/workdir');
            console.log("Created /workdir directory");
        }

        FS.mount(IDBFS, { autoPersist: true }, '/workdir');

        FS.syncfs(true, function (err) {
            console.log(FS.readdir('/workdir'));
            if (err) {
                console.error(err);
            } else {
                console.log("Synced file system");
                Module.ccall('onSyncfsFinished', 'v');
            }
        });
    );
#endif
}

bool FileSystem::fileExists(const std::string& fileName) {
#if defined(__EMSCRIPTEN__)
    const auto filePath = "/workdir/" + fileName;
    bool result = FileExists_Emscripten(filePath.c_str());
    return result;
#else
    const std::ifstream file(fileName);
    return file.good();
#endif
}

std::vector<char> FileSystem::readFile(const std::string& fileName) {
#if defined(__EMSCRIPTEN__)
    const auto filePath = "/workdir/" + fileName;
    std::vector<char> buffer;
    ReadFile_Emscripten(filePath.c_str(), &buffer);
    return buffer;
#else
    return readFileNative(fileName);
#endif
}

std::vector<char> FileSystem::readFileNative(const std::string &fileName) {
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
}

void FileSystem::cleanFiles(const std::string& extension) {
    std::string ext = extension;
    if (!ext.empty() && ext.front() != '.')
        ext.insert(ext.begin(), '.');      // make sure it starts with '.'

#if defined(__EMSCRIPTEN__)
    MAIN_THREAD_EM_ASM({
        const dir   = "/workdir";
        const ext   = UTF8ToString($0);
        const files = FS.readdir(dir);

        files.forEach(name => {
            if (name === "." || name === "..") return;
            if (name.endsWith(ext)) {
                try { FS.unlink(dir + "/" + name); }
                catch (e) { /* ignore errors */ }
            }
        });
    }, ext.c_str());

#else
    namespace Fs = std::filesystem;

    try {
        for (const auto& entry : Fs::directory_iterator(Fs::current_path())) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ext) {
                std::error_code ec;
                Fs::remove(entry, ec);
                if (ec) {
                    LogCore::warning("Failed to remove {0}: {1}", entry.path().string(), ec.message());
                }
            }
        }
    }
    catch (const std::exception& e) {
        LogCore::error("CleanFiles exception: {0}", e.what());
    }
#endif
}


void FileSystem::writeFile(const std::string& fileName, const char* buffer, const size_t size) {
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

void FileSystem::ensureDirectory(const std::string& path) {
    if (path.empty()) {
        return;
    }

#if defined(__EMSCRIPTEN__)
    MAIN_THREAD_EM_ASM({
        const rawPath = UTF8ToString($0);
        if (!rawPath) return;

        const parts = rawPath.split("/").filter(Boolean);
        let current = "workdir";
        for (const part of parts) {
            current += "/" + part;
            try {
                if (!FS.analyzePath(current).exists) {
                    FS.mkdir(current);
                }
            } catch (e) {
                // Ignore "already exists" and intermediate path races.
            }
        }
    }, path.c_str());
#else
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        LogCore::error("Failed to create directory {0}: {1}", path, ec.message());
    }
#endif
}

std::vector<std::string> FileSystem::listDirectories(const std::string& path)
{
    std::vector<std::string> result;

#if defined(__EMSCRIPTEN__)

    char buffer[8192] = {};

    MAIN_THREAD_EM_ASM({

        let path = UTF8ToString($0);


        // Application filesystem root is /workdir
        // Convert relative paths to IDBFS paths
        if (!path.startsWith("/workdir"))
        {
            if (path.startsWith("./"))
            {
                path = path.substring(2);
            }

            path = "/workdir/" + path;
        }


        console.log("Listing directory:", path);


        try
        {
            const entries =
                FS.readdir(path);


            const directories = [];


            for (const entry of entries)
            {
                if (entry === "." || entry === "..")
                    continue;


                const fullPath =
                    path + "/" + entry;


                const stat =
                    FS.stat(fullPath);


                if (FS.isDir(stat.mode))
                {
                    directories.push(entry);
                }
            }


            stringToUTF8(
                directories.join("\n"),
                $1,
                $2
            );

        }
        catch (e)
        {
            console.error(
                "Failed to list directories:",
                path,
                e
            );


            stringToUTF8(
                "",
                $1,
                $2
            );
        }


    },
    path.c_str(),
    buffer,
    sizeof(buffer));


    std::string data(buffer);


    size_t start = 0;


    while (start < data.size())
    {
        const auto end =
            data.find('\n', start);


        if (end == std::string::npos)
        {
            if (!data.substr(start).empty())
            {
                result.push_back(
                    data.substr(start)
                );
            }

            break;
        }


        result.push_back(
            data.substr(start, end - start)
        );


        start = end + 1;
    }


#else

    std::error_code ec;


    for (const auto& entry :
         std::filesystem::directory_iterator(path, ec))
    {
        if (entry.is_directory())
        {
            result.push_back(
                entry.path().filename().string()
            );
        }
    }


#endif


    return result;
}

void FileSystem::download(const std::string& fileName, const std::string& downloadName) {
#if defined(__EMSCRIPTEN__)
    const auto filePath = "/workdir/" + fileName;
    DownloadFile_Emscripten(filePath.c_str(), downloadName.c_str());
#else
    LogCore::error("Download is only supported in Emscripten builds.");
#endif
}
