#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define WEBVOXEL_API __declspec(dllexport)
#else
#define WEBVOXEL_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WebVoxelGenerator WebVoxelGenerator;

WEBVOXEL_API WebVoxelGenerator* webvoxel_generator_create(int32_t seed, int32_t caves_enabled);
WEBVOXEL_API void webvoxel_generator_destroy(WebVoxelGenerator* generator);

// format: 0 = JSON, 1 = engine-compatible compressed binary.
// Returns zero on success. The caller must release output with webvoxel_free.
WEBVOXEL_API int32_t webvoxel_generate_chunk(
    WebVoxelGenerator* generator,
    int32_t x,
    int32_t y,
    int32_t z,
    int32_t format,
    uint8_t** output,
    size_t* output_size);

WEBVOXEL_API void webvoxel_free(void* memory);
WEBVOXEL_API const char* webvoxel_last_error(void);

#ifdef __cplusplus
}
#endif
