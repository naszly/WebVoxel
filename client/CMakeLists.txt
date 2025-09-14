cmake_minimum_required(VERSION 3.28)

project(WebVoxel)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

if (EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msimd128 -matomics -mbulk-memory -pthread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msimd128 -matomics -mbulk-memory -pthread")
endif ()

include(FetchContent)

# GLM
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

# spdlog
set(SPDLOG_USE_STD_FORMAT ON)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.14.1
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(spdlog)

# Magic Enum
FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum
        GIT_TAG v0.9.6
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(magic_enum)

# ImGUI
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
        ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp)

# FastNoise2
FetchContent_Declare(
        FastNoise2
        GIT_REPOSITORY https://github.com/uriel-4/FastNoise2
        GIT_TAG master
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(FastNoise2)

# ZLIB
set(ZLIB_BUILD_EXAMPLES OFF)
FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib
        GIT_TAG v1.3.1
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(zlib)

# opengametools (ogt_vox.h)
FetchContent_Declare(
        opengametools
        GIT_REPOSITORY https://github.com/jpaver/opengametools
        GIT_TAG master
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(opengametools)
file(GLOB_RECURSE OGT_SOURCES ${opengametools_SOURCE_DIR}/src/*)

# stb image
FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb
        GIT_TAG master
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(stb)

# cxxopts
FetchContent_Declare(
        cxxopts
        GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
        GIT_TAG v3.1.1  # Latest stable as of 2025
        GIT_SHALLOW ON
)
FetchContent_MakeAvailable(cxxopts)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES} ${OGT_SOURCES})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_WARNING_AS_ERROR ON)

target_link_libraries(${PROJECT_NAME} glm::glm webgpu glfw glfw3webgpu spdlog::spdlog magic_enum::magic_enum FastNoise2 zlib cxxopts)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PRIVATE ${webgpu_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${glfw3webgpu_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_include_directories(${PROJECT_NAME} PRIVATE ${opengametools_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PRIVATE ${stb_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${cxxopts_SOURCE_DIR}/include)

target_compile_definitions(
        ${PROJECT_NAME} PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_LEFT_HANDED
        GLM_FORCE_RADIANS
        GLM_ENABLE_EXPERIMENTAL
        GLFW_INCLUDE_NONE
        IMGUI_IMPL_WEBGPU_BACKEND_DAWN
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        S_DEBUGGER_ENVIRONMENT "DAWN_DEBUG_BREAK_ON_ERROR=1"
)

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

if (EMSCRIPTEN)
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html" OUTPUT_NAME "index")
    target_link_options(${PROJECT_NAME} PRIVATE
            -sASSERTIONS=2
            -sSTACK_OVERFLOW_CHECK=2
            -sUSE_GLFW=3
            -sASYNCIFY
            -sSTACK_SIZE=8388608 # 8MB
            -sALLOW_MEMORY_GROWTH
            -sINITIAL_MEMORY=2147483648 # 2GB
            -sMAXIMUM_MEMORY=4294967296 # 4GB
            -sEXIT_RUNTIME
            -sUSE_PTHREADS
            -sPTHREAD_POOL_SIZE=6
            -sFORCE_FILESYSTEM=1
            -lidbfs.js
            -sEXPORTED_RUNTIME_METHODS=requestFullscreen,ccall,cwrap
            -sEXPORTED_FUNCTIONS=_main,_onSyncfsFinished
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "--preload-file ${ASSET_DIR}@/ --shell-file ${PROJECT_SOURCE_DIR}/shell.html")
endif ()