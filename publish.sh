#!/bin/bash

set -ex

mkdir -p publish

rm -f build-web/client/index.*
rm -f build-web/client/game.*

emcmake cmake -B build-web
cmake --build build-web

rm -rf publish/*

cp -v build-web/client/index.* publish/
cp -v build-web/client/game.* publish/

cat <<EOL > publish/_headers
/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
EOL