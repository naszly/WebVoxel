#!/bin/bash

if [[ $* == *"-w"* ]]; then
    emcmake cmake -B build-web
    cmake --build build-web
else
    cmake -B build-desktop
    cmake --build build-desktop
fi
