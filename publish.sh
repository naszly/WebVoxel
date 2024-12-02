#!/bin/bash

mkdir -p publish

rm -f build-emscripten/index.*

emcmake cmake -B build-emscripten
cmake --build build-emscripten

rm -rf publish/*

cp -v build-emscripten/index.* publish/

cat <<EOL > publish/_headers
/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
EOL