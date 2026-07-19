#!/usr/bin/env bash
#
# Fetch and build a lean static SDL3 (video only) into an install prefix.
# Shared helper used by the per-platform build scripts.
#
# Usage: build-sdl3.sh <install-prefix> [extra cmake args ...]
#
set -euo pipefail

SDL_VER=3.4.12
here=$(cd "$(dirname "$0")/.." && pwd)
cache="$here/build-deps"
prefix="$1"; shift

mkdir -p "$cache"
src="$cache/SDL3-$SDL_VER"
if [ ! -d "$src" ]; then
    echo ">> downloading SDL3 $SDL_VER"
    curl -Lf -o "$cache/SDL3-$SDL_VER.tar.gz" \
        "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL3-$SDL_VER.tar.gz"
    tar xzf "$cache/SDL3-$SDL_VER.tar.gz" -C "$cache"
fi

# Reuse an existing install (delete the prefix to force a rebuild).
if [ -f "$prefix/lib/libSDL3.a" ] || [ -f "$prefix/lib64/libSDL3.a" ]; then
    echo ">> SDL3 already built at $prefix"
    exit 0
fi

build="$cache/build-$(basename "$prefix")"
rm -rf "$build"

njobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)

echo ">> configuring SDL3 -> $prefix"
cmake -S "$src" -B "$build" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF \
    -DSDL_AUDIO=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF \
    -DSDL_CAMERA=OFF -DSDL_SENSOR=OFF \
    -DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF \
    "$@"
cmake --build "$build" -j"$njobs"
cmake --install "$build"
echo ">> SDL3 installed to $prefix"
