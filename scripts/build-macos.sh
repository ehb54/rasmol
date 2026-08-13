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

# Ad-hoc sign both slices. The linker already signs arm64 (macOS requires a
# valid signature to execute an arm64 binary at all) but leaves x86_64
# unsigned, so `codesign --verify` fails on the fat file even though each
# slice runs. Sign explicitly so the artifact verifies as a whole, so a
# `strip` that does not preserve the linker's signature cannot silently break
# arm64, and so a future move to Developer ID signing starts from a signed
# binary. Must come after strip -- stripping rewrites the Mach-O.
codesign --force --sign - "$out/rasmol"

cp "$here/doc/rasmol.hlp" "$out/rasmol.hlp"

echo "==> $out/rasmol"
lipo -info "$out/rasmol"
codesign --verify --strict --verbose=2 "$out/rasmol"
