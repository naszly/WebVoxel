cmake_minimum_required(VERSION 3.28)

project(WebVoxelChunkGeneratorApi LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW ON
    UPDATE_DISCONNECTED ON
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    abseil-cpp
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 20250512.1
    GIT_SHALLOW ON
    UPDATE_DISCONNECTED ON
)
FetchContent_MakeAvailable(abseil-cpp)

FetchContent_Declare(
    FastNoise2
    GIT_REPOSITORY https://github.com/uriel-4/FastNoise2
    GIT_TAG master
    GIT_SHALLOW ON
    UPDATE_DISCONNECTED ON
)
FetchContent_MakeAvailable(FastNoise2)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG v1.14.1
    GIT_SHALLOW ON
    UPDATE_DISCONNECTED ON
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib
    GIT_TAG v1.3.1
    GIT_SHALLOW ON
    UPDATE_DISCONNECTED ON
)
set(ZLIB_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(zlib)

add_executable(world-generator-api
    chunk_generator_main.cpp
    ../client/src/application/world/WorldGenerator.cpp
    ../client/src/application/world/chunk/Chunk.cpp
    ../client/src/application/domain/Block.cpp
    ../client/src/application/common/CompressionUtils.cpp
    ../client/src/common/FileSystem.cpp
    ../client/src/common/Log.cpp
    ../client/src/common/datastructures/GlobalBlockAllocator.cpp
)

target_compile_features(world-generator-api PRIVATE cxx_std_23)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(world-generator-api PRIVATE -ffunction-sections)
    target_link_options(world-generator-api PRIVATE -Wl,--gc-sections)
endif ()
target_include_directories(world-generator-api PRIVATE ../client/src)
target_link_libraries(world-generator-api PRIVATE
    glm::glm
    absl::flat_hash_map
    absl::hash
    FastNoise2
    spdlog::spdlog
    zlibstatic
)
