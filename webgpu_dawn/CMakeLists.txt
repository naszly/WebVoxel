cmake_minimum_required(VERSION 3.0.0...3.24 FATAL_ERROR)
project(webgpu-backend-dawn VERSION 1.0.0)

if (EMSCRIPTEN)

    message(STATUS "Using emdawnwebgpu backend for WebGPU")

    set(EMDAWNWEBGPU_VERSION "v20250712.164425" CACHE STRING "Version of the emdawnwebgpu to use. Must correspond to a valid tag on EMDAWNWEBGPU_MIRROR.")

    set(EMDAWNWEBGPU_MIRROR "https://github.com/google/dawn" CACHE STRING "The repository where to find emdawnwebgpu precompiled releases.")

    include(FetchContent)

    set(URL "${EMDAWNWEBGPU_MIRROR}/releases/download/${EMDAWNWEBGPU_VERSION}/emdawnwebgpu_pkg-${EMDAWNWEBGPU_VERSION}.zip")
    set(FC_NAME "emdawnwebgpu")
    FetchContent_Declare(${FC_NAME} URL ${URL})
    FetchContent_MakeAvailable(${FC_NAME})
    set(emdawnwebgpu_ROOT "${${FC_NAME}_SOURCE_DIR}")

    message(STATUS "Using emdawnwebgpu from ${emdawnwebgpu_ROOT}")

    add_library(webgpu INTERFACE)

    # This is used to advertise the flavor of WebGPU that this zip provides
    target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_EMDAWNWEBGPU)

    target_compile_options(webgpu INTERFACE "--use-port=${emdawnwebgpu_ROOT}/emdawnwebgpu.port.py")

    target_link_options(webgpu INTERFACE "--use-port=${emdawnwebgpu_ROOT}/emdawnwebgpu.port.py")

    function(target_copy_webgpu_binaries Target)
    endfunction()

else (EMSCRIPTEN)

    message(STATUS "Using Dawn backend for WebGPU")

    add_library(webgpu INTERFACE)

    include(cmake/FetchDawn.cmake)

    target_link_libraries(webgpu INTERFACE webgpu_dawn)
    target_include_directories(webgpu INTERFACE
            "${dawn_SOURCE_DIR}/include"
    )

    # This is used to advertise the flavor of WebGPU that this zip provides
    target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_DAWN)

    function(target_copy_webgpu_binaries Target)
        add_custom_command(
                TARGET ${Target} POST_BUILD
                COMMAND
                "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE_DIR:webgpu_dawn>/$<TARGET_FILE_NAME:webgpu_dawn>"
                "$<TARGET_FILE_DIR:${Target}>"
                COMMENT
                "Copying '$<TARGET_FILE_DIR:webgpu_dawn>/$<TARGET_FILE_NAME:webgpu_dawn>' to '$<TARGET_FILE_DIR:${Target}>'..."
        )
    endfunction()

endif (EMSCRIPTEN)
