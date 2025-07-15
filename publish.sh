#!/bin/bash

mkdir -p publish

rm -f build-web/client/index.*

emcmake cmake -B build-web
cmake --build build-web

rm -rf publish/*

cp -v build-web/client/index.* publish/

cat <<EOL > publish/_headers
/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
EOL