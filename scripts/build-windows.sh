#!/usr/bin/env bash
#
# Cross-compile the Windows x86_64 binary with MinGW-w64 and stage it into
# binaries/windows-x86_64/. Run on macOS or Linux with mingw-w64 installed:
#   macOS:  brew install mingw-w64
#   Debian: apt install g++-mingw-w64-x86-64
#   Fedora: dnf install mingw64-gcc-c++
#
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
toolchain="$here/cmake/mingw-toolchain.cmake"

sdl="$here/build-deps/sdl3-mingw"
"$here/scripts/build-sdl3.sh" "$sdl" -DCMAKE_TOOLCHAIN_FILE="$toolchain"

build="$here/build-mingw"
cmake -S "$here" -B "$build" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
    -DCMAKE_PREFIX_PATH="$sdl" \
    -DCMAKE_EXE_LINKER_FLAGS="-static"
cmake --build "$build" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

out="$here/binaries/windows-x86_64"
mkdir -p "$out"
cp "$build/rasmol.exe" "$out/rasmol.exe"
x86_64-w64-mingw32-strip "$out/rasmol.exe"
cp "$here/doc/rasmol.hlp" "$out/rasmol.hlp"

echo "==> $out/rasmol.exe"
file "$out/rasmol.exe"
