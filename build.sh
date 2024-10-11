#!/bin/bash

if [[ $* == *"-w"* ]]; then
    emcmake cmake -B build-emscripten
    cmake --build build-emscripten
else
    cmake -B build-wgpu -DWEBGPU_BACKEND=WGPU
    cmake --build build-wgpu
fi
