#!/usr/bin/env bash
#
# Build the macOS universal (arm64 + x86_64) binary and stage it into
# binaries/macos-universal/. Run on macOS. Requires cmake.
#
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)

sdl="$here/build-deps/sdl3-macos"
"$here/scripts/build-sdl3.sh" "$sdl" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

build="$here/build-macos"
cmake -S "$here" -B "$build" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$sdl" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build "$build" -j"$(sysctl -n hw.ncpu)"

out="$here/binaries/macos-universal"
mkdir -p "$out"
cp "$build/rasmol" "$out/rasmol"
strip -x "$out/rasmol"
cp "$here/doc/rasmol.hlp" "$out/rasmol.hlp"

echo "==> $out/rasmol"
lipo -info "$out/rasmol"
