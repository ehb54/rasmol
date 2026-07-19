#!/usr/bin/env bash
#
# Build the Linux x86_64 binary and stage it into binaries/linux-x86_64/.
# Run on Linux. Requires cmake and X11 dev headers (see README).
#
# For maximum compatibility, build on the OLDEST glibc you want to support
# (a binary built against old glibc runs on that and every newer version).
#
# Use a modern compiler if the system default is too old for C++17:
#   Rocky/RHEL 8:   scl enable gcc-toolset-13 scripts/build-linux.sh
#   Ubuntu 16.04:   CC=gcc-9 CXX=g++-9 scripts/build-linux.sh
#
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)

sdl="$here/build-deps/sdl3-linux"
"$here/scripts/build-sdl3.sh" "$sdl"

build="$here/build-linux"
cmake -S "$here" -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$sdl"
cmake --build "$build" -j"$(nproc)"

out="$here/binaries/linux-x86_64"
mkdir -p "$out"
cp "$build/rasmol" "$out/rasmol"
strip "$out/rasmol"
cp "$here/doc/rasmol.hlp" "$out/rasmol.hlp"

echo "==> $out/rasmol"
echo "    glibc floor: $(objdump -T "$out/rasmol" | grep -oE 'GLIBC_[0-9.]+' | sort -V | tail -1)"
