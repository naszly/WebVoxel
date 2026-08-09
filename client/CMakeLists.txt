cmake_minimum_required(VERSION 3.28)

project(WebVoxel)

option(WEBVOXEL_USE_CHUNK_API "Load missing WebAssembly chunks from the server API" OFF)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

if (EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msimd128 -matomics -mbulk-memory -pthread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msimd128 -matomics -mbulk-memory -pthread")
endif ()

include(FetchContent)


# -------------------------
# Dependencies
# -------------------------

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
        glfw3webgpu
        GIT_REPOSITORY https://github.com/naszly/glfw3webgpu
        GIT_TAG main
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(glfw3webgpu)

set(SPDLOG_USE_STD_FORMAT ON)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.14.1
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum
        GIT_TAG v0.9.6
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(magic_enum)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.1-docking
        GIT_SHALLOW ON
        PATCH_COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/patches/imgui_impl_wgpu.patch > /dev/null 2>&1 || true
        UPDATE_DISCONNECTED 1
)
FetchContent_MakeAvailable(imgui)

file(GLOB IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/*.cpp
        ${imgui_SOURCE_DIR}/*.h
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
)

FetchContent_Declare(
        FastNoise2
        GIT_REPOSITORY https://github.com/uriel-4/FastNoise2
        GIT_TAG master
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(FastNoise2)

set(ZLIB_BUILD_EXAMPLES OFF)
FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib
        GIT_TAG v1.3.1
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(zlib)

FetchContent_Declare(
        opengametools
        GIT_REPOSITORY https://github.com/jpaver/opengametools
        GIT_TAG master
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(opengametools)
file(GLOB_RECURSE OGT_SOURCES ${opengametools_SOURCE_DIR}/src/*)

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb
        GIT_TAG master
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
        cxxopts
        GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
        GIT_TAG v3.1.1  # Latest stable as of 2025
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(cxxopts)



# -------------------------
# Shared filesystem sources
# -------------------------

file(GLOB_RECURSE FILESYSTEM_SOURCES
        CONFIGURE_DEPENDS
        src/common/*.cpp
        src/common/*.h
)



# -------------------------
# GAME
# -------------------------

file(GLOB_RECURSE GAME_SOURCES
        CONFIGURE_DEPENDS
        src/*.cpp
        src/*.h
)

list(REMOVE_ITEM GAME_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/launcher_main.cpp
)


add_executable(
        WebVoxel
        ${GAME_SOURCES}
        ${IMGUI_SOURCES}
        ${OGT_SOURCES}
)


set_property(
        TARGET WebVoxel
        PROPERTY CXX_STANDARD 23
)


target_link_libraries(
        WebVoxel
        glm::glm
        webgpu
        glfw
        glfw3webgpu
        spdlog::spdlog
        magic_enum::magic_enum
        FastNoise2
        cxxopts
        zlibstatic
)



target_include_directories(
        WebVoxel PRIVATE

        src

        ${webgpu_SOURCE_DIR}
        ${glfw3webgpu_SOURCE_DIR}

        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends

        ${opengametools_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}

        ${cxxopts_SOURCE_DIR}/include

        ${zlib_SOURCE_DIR}
        ${zlib_BINARY_DIR}
)



target_compile_definitions(
        WebVoxel PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_LEFT_HANDED
        GLM_FORCE_RADIANS
        GLM_ENABLE_EXPERIMENTAL
        GLFW_INCLUDE_NONE
        IMGUI_IMPL_WEBGPU_BACKEND_DAWN
)

if (EMSCRIPTEN AND WEBVOXEL_USE_CHUNK_API)
    target_compile_definitions(WebVoxel PRIVATE WEBVOXEL_USE_CHUNK_API=1)
endif ()


if (EMSCRIPTEN)
    set(ASSET_DIR ${PROJECT_BINARY_DIR}/asset_dir)
else ()
    set(ASSET_DIR ${PROJECT_BINARY_DIR})
endif ()

file(GLOB_RECURSE SHADER_FILES ${PROJECT_SOURCE_DIR}/shaders/*)
foreach (SHADER_FILE ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    configure_file(${SHADER_FILE} ${ASSET_DIR}/shaders/${SHADER_NAME} COPYONLY)
endforeach ()

file(GLOB_RECURSE MODEL_FILES ${PROJECT_SOURCE_DIR}/models/*)
foreach (MODEL_FILE ${MODEL_FILES})
    get_filename_component(MODEL_NAME ${MODEL_FILE} NAME)
    configure_file(${MODEL_FILE} ${ASSET_DIR}/models/${MODEL_NAME} COPYONLY)
endforeach ()

file(GLOB_RECURSE TEXTURE_FILES ${PROJECT_SOURCE_DIR}/textures/*)
foreach (TEXTURE_FILE ${TEXTURE_FILES})
    get_filename_component(TEXTURE_NAME ${TEXTURE_FILE} NAME)
    configure_file(${TEXTURE_FILE} ${ASSET_DIR}/textures/${TEXTURE_NAME} COPYONLY)
endforeach ()

file(GLOB_RECURSE FONT_FILES ${imgui_SOURCE_DIR}/misc/fonts/*.ttf)
foreach (FONT_FILE ${FONT_FILES})
    get_filename_component(FONT_NAME ${FONT_FILE} NAME)
    configure_file(${FONT_FILE} ${ASSET_DIR}/fonts/${FONT_NAME} COPYONLY)
endforeach ()



# -------------------------
# LAUNCHER
# -------------------------

add_executable(WebVoxelLauncher
        src/launcher_main.cpp
        src/common/FileSystem.cpp
        src/common/Log.cpp
        src/common/Timer.cpp
        #src/common/datastructures/GlobalBlockAllocator.cpp
)

set_property(
        TARGET WebVoxelLauncher
        PROPERTY CXX_STANDARD 23
)

target_link_libraries(
        WebVoxelLauncher
        spdlog::spdlog
        absl::hash
        absl::raw_hash_set
        glm::glm
)

target_include_directories(
        WebVoxelLauncher PRIVATE

        src
)



# -------------------------
# Emscripten output
# -------------------------

if (EMSCRIPTEN)


    #
    # GAME
    #

    set_target_properties(
            WebVoxel
            PROPERTIES

            SUFFIX ".html"

            OUTPUT_NAME "game"
    )


    target_link_options(
            WebVoxel PRIVATE

            -sASSERTIONS=2
            -sSTACK_OVERFLOW_CHECK=2
            -sUSE_GLFW=3
            -sASYNCIFY
            -sSTACK_SIZE=8388608 # 8MB
            -sALLOW_MEMORY_GROWTH
            -sINITIAL_MEMORY=536870912      # 512 MB
            -sMAXIMUM_MEMORY=4294967296     # 4 GB
            -sEXIT_RUNTIME
            -sUSE_PTHREADS
            -sPTHREAD_POOL_SIZE=6
            -sFORCE_FILESYSTEM=1
            -lidbfs.js
            -sEXPORTED_RUNTIME_METHODS=requestFullscreen,ccall,cwrap
            -sEXPORTED_FUNCTIONS=_main,_onSyncfsFinished,_onNavigationAway,_applyRemoteBlockEdit
    )

    if (WEBVOXEL_USE_CHUNK_API)
        target_link_options(WebVoxel PRIVATE -sFETCH)
    endif ()


    set_target_properties(
            WebVoxel
            PROPERTIES

            LINK_FLAGS
            "--preload-file ${PROJECT_BINARY_DIR}/asset_dir@/ --shell-file ${PROJECT_SOURCE_DIR}/game_shell.html"
    )



    #
    # LAUNCHER
    #

    set_target_properties(
            WebVoxelLauncher

            PROPERTIES

            SUFFIX ".html"
            OUTPUT_NAME "index"
    )


    target_link_options(
            WebVoxelLauncher PRIVATE
            -sASSERTIONS=2
            -sASYNCIFY
            -sALLOW_MEMORY_GROWTH
            -sINITIAL_MEMORY=67108864
            -sFORCE_FILESYSTEM=1
            -lidbfs.js
            -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,FS
            -sEXPORTED_FUNCTIONS=_main,_onSyncfsFinished,_getWorldCount,_getWorldName
            -sNO_EXIT_RUNTIME=1
    )


    set_target_properties(
            WebVoxelLauncher

            PROPERTIES

            LINK_FLAGS
            "--shell-file ${PROJECT_SOURCE_DIR}/index_shell.html"
    )


endif()
