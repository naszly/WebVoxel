# Setup

This document shows the minimal commands to build the project natively with GCC and to build the web (WASM) version with Emscripten.

Prerequisites

- Native: gcc, g++, cmake
- Web: Emscripten SDK (emsdk) available on PATH (provides emcmake, em++/emcc) OR Docker

## Build native (Linux, GCC)

From the project root:

```bash
# configure with GCC/g++
cmake -S . -B build-native -G "Unix Makefiles"  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++

# build
cmake --build build-native -- -j$(nproc)
```

Notes:
- If you prefer Ninja, omit `-G "Unix Makefiles"`; Ninja also works with GCC/g++.

## Build web (Emscripten -> WASM + HTML)

Local build with `emsdk` active:

```bash
emcmake cmake -S . -B build-web
cmake --build build-web
# web output is generated under `build-web/client` (for example `index.html`)
```

Serve the web build locally with `server.py`:

```bash
python3 server.py build-web/client localhost
```

## Inside Docker (alternative):

```bash
# build the Docker image (runs `publish.sh` during build; the Dockerfile uses `emscripten/emsdk`)
docker build -t webvoxel-builder .
```

Run the web server in Docker:

```bash
docker run -p 8000:8000 webvoxel-builder
# opens web server at http://localhost:8000
```

Or extract the build and serve locally:

```bash
# copy the published web files out of the container
docker create --name temp-build webvoxel-builder
docker cp temp-build:/app/publish ./publish-output
docker rm temp-build

# serve locally with server.py
python3 server.py publish-output localhost
```
