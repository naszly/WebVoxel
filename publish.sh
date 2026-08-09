#!/bin/bash

set -ex

publish_dir="${1:-publish}"
if (($# > 0)); then
    shift
fi

mkdir -p "$publish_dir"

rm -f build-web/client/index.*
rm -f build-web/client/game.*

emcmake cmake -B build-web "$@"
cmake --build build-web

rm -rf "${publish_dir:?}/"*

cp -v build-web/client/index.* "$publish_dir"/
cp -v build-web/client/game.* "$publish_dir"/

cat <<EOL > "$publish_dir"/_headers
/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
EOL
