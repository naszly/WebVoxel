#include "common/FileSystem.h"

#include <vector>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
extern void waitInitialSyncfs();
#endif

static std::vector<std::string> g_worlds;

extern "C"
{

EMSCRIPTEN_KEEPALIVE
int getWorldCount()
{
    return static_cast<int>(g_worlds.size());
}

EMSCRIPTEN_KEEPALIVE
const char* getWorldName(int index)
{
    if (index < 0 || index >= static_cast<int>(g_worlds.size()))
        return "";

    return g_worlds[index].c_str();
}

}

int main()
{
    FileSystem::initialize();

#ifdef __EMSCRIPTEN__
    waitInitialSyncfs();
#endif

    FileSystem::ensureDirectory("worlds");

    g_worlds =
        FileSystem::listDirectories("worlds");

#ifdef __EMSCRIPTEN__
    EM_ASM({
        if (window.onWorldListReady)
            window.onWorldListReady();
    });

    emscripten_exit_with_live_runtime();
#endif

    return 0;
}