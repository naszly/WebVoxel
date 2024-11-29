cmake_minimum_required(VERSION 3.13)

project(WebVoxel)

set(CMAKE_CXX_STANDARD 23
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

if (EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msimd128 -matomics -mbulk-memory")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msimd128 -matomics -mbulk-memory")
endif()

include(FetchContent)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(glm)

# GLFW
if (NOT EMSCRIPTEN)
    FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw
            GIT_TAG 3.4
            GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(glfw)
endif()

# WebGPU
FetchContent_Declare(
    webgpu
    GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
    GIT_TAG dawn
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(webgpu)

FetchContent_Declare(
    glfw3webgpu
    GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu
    GIT_TAG main
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(glfw3webgpu)

include(FetchContent)

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
FetchContent_Populate(
        imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.2-docking.zip
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)
file(GLOB IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/*.cpp
        ${imgui_SOURCE_DIR}/*.h
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp)


file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

target_link_libraries(${PROJECT_NAME} glm::glm webgpu glfw glfw3webgpu spdlog::spdlog magic_enum::magic_enum)
target_include_directories(${PROJECT_NAME} PRIVATE ${webgpu_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${glfw3webgpu_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

target_compile_definitions(
    ${PROJECT_NAME} PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_LEFT_HANDED
    GLM_FORCE_RADIANS
    GLM_ENABLE_EXPERIMENTAL
    GLFW_INCLUDE_NONE
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    S_DEBUGGER_ENVIRONMENT "DAWN_DEBUG_BREAK_ON_ERROR=1"
)

if (EMSCRIPTEN)
   set(ASSET_DIR ${CMAKE_BINARY_DIR}/asset_dir)
else()
    set(ASSET_DIR ${CMAKE_BINARY_DIR})
endif()

file(GLOB_RECURSE SHADERS_FILES ${CMAKE_SOURCE_DIR}/shaders/*)
foreach(SHADER_FILE ${SHADERS_FILES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    configure_file(${SHADER_FILE} ${ASSET_DIR}/shaders/${SHADER_NAME} COPYONLY)
endforeach()

file(GLOB_RECURSE FONT_FILES ${CMAKE_SOURCE_DIR}/external/imgui/misc/fonts/*.ttf)
foreach(FONT_FILE ${FONT_FILES})
    get_filename_component(FONT_NAME ${FONT_FILE} NAME)
    configure_file(${FONT_FILE} ${ASSET_DIR}/fonts/${FONT_NAME} COPYONLY)
endforeach()

if(EMSCRIPTEN)
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html" OUTPUT_NAME "index")
    target_link_options(${PROJECT_NAME} PRIVATE
            -sASSERTIONS=2
            -sSTACK_OVERFLOW_CHECK=2
            -sUSE_GLFW=3
            -sUSE_WEBGPU=1
            -sASYNCIFY
            -sSTACK_SIZE=5242880 # 5MB
            #-sINITIAL_HEAP=268435456 # 256MB
            -sALLOW_MEMORY_GROWTH
            -sEXIT_RUNTIME
            -sWASM_WORKERS
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "--preload-file ${ASSET_DIR}@/ --shell-file ${CMAKE_SOURCE_DIR}/shell.html")
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
endif()