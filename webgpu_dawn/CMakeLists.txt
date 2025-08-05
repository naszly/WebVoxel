cmake_minimum_required(VERSION 3.28)
project(webgpu VERSION 1.0.0)

include(FetchContent)

if (EMSCRIPTEN)

    message(STATUS "Using emdawnwebgpu backend for WebGPU")

    set(EMDAWNWEBGPU_VERSION "v20250712.164425" CACHE STRING "Version of the emdawnwebgpu to use. Must correspond to a valid tag on EMDAWNWEBGPU_MIRROR.")

    set(EMDAWNWEBGPU_MIRROR "https://github.com/google/dawn" CACHE STRING "The repository where to find emdawnwebgpu precompiled releases.")

    set(URL "${EMDAWNWEBGPU_MIRROR}/releases/download/${EMDAWNWEBGPU_VERSION}/emdawnwebgpu_pkg-${EMDAWNWEBGPU_VERSION}.zip")
    set(FC_NAME "emdawnwebgpu")
    FetchContent_Declare(${FC_NAME} URL ${URL})
    FetchContent_MakeAvailable(${FC_NAME})
    set(emdawnwebgpu_ROOT "${${FC_NAME}_SOURCE_DIR}")

    message(STATUS "Using emdawnwebgpu from ${emdawnwebgpu_ROOT}")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -matomics -mbulk-memory")

    # abseil-cpp
    FetchContent_Declare(
            abseil-cpp
            GIT_REPOSITORY https://github.com/abseil/abseil-cpp
            GIT_TAG 20250512.1
            GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(abseil-cpp)

    add_library(${PROJECT_NAME} INTERFACE)
    target_link_libraries(${PROJECT_NAME} INTERFACE absl::flat_hash_map absl::flat_hash_set absl::hash)

    # This is used to advertise the flavor of WebGPU that this zip provides
    target_compile_definitions(${PROJECT_NAME} INTERFACE WEBGPU_BACKEND_EMDAWNWEBGPU)

    target_compile_options(${PROJECT_NAME} INTERFACE "--use-port=${emdawnwebgpu_ROOT}/emdawnwebgpu.port.py")

    target_link_options(${PROJECT_NAME} INTERFACE "--use-port=${emdawnwebgpu_ROOT}/emdawnwebgpu.port.py")

else ()

    message(STATUS "Using Dawn backend for WebGPU")

    set(FETCHCONTENT_QUIET FALSE)

    FetchContent_Declare(
        dawn
        GIT_REPOSITORY https://dawn.googlesource.com/dawn
        GIT_TAG chromium/7339
        GIT_SHALLOW ON
        GIT_PROGRESS TRUE
        GIT_SUBMODULES
            third_party/markupsafe
    )

    FetchContent_GetProperties(dawn)
    if (NOT dawn_POPULATED)
        message(STATUS "Populating Dawn...")
        FetchContent_Populate(dawn)

        set(DAWN_FETCH_DEPENDENCIES ON)
        set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC)

        set(DAWN_ENABLE_D3D11 OFF)
        set(DAWN_ENABLE_D3D12 OFF)
        set(DAWN_ENABLE_METAL OFF)
        set(DAWN_ENABLE_NULL OFF)
        set(DAWN_ENABLE_OPENGLES OFF)
        set(DAWN_ENABLE_DESKTOP_GL OFF)
        set(DAWN_ENABLE_VULKAN ON)
        set(DAWN_BUILD_SAMPLES OFF)
        set(TINT_BUILD_CMD_TOOLS OFF)
        set(TINT_BUILD_TESTS OFF)
        set(TINT_BUILD_IR_BINARY OFF)
        set(TINT_BUILD_SPV_READER OFF)

        add_subdirectory(${dawn_SOURCE_DIR} ${dawn_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif ()

    add_library(${PROJECT_NAME} INTERFACE)
    target_link_libraries(${PROJECT_NAME} INTERFACE dawn::webgpu_dawn
            absl::flat_hash_map absl::flat_hash_set absl::hash)
    target_include_directories(${PROJECT_NAME} INTERFACE
            ${DAWN_INCLUDE_DIR}
            ${DAWN_BUILD_GEN_DIR}/include
    )

endif ()