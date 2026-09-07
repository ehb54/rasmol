#!/usr/bin/env bash
#
# Build the Linux x86_64 binary inside the pinned glibc 2.17 container and
# stage it into binaries/linux-x86_64/. Run from any machine with Docker; the
# host distribution does not matter.
#
# Why a container: a binary inherits the glibc floor of whatever it was linked
# against, and the shipped Linux binary has always had a GLIBC_2.17 floor. That
# used to come from one old build box, which made the floor a property of a
# machine that could be upgraded out from under the release. The floor now
# lives in scripts/Dockerfile.linux -- read the note there before changing it.
#
# Usage:
#   scripts/build-linux-docker.sh
#
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)

image=${RASMOL_LINUX_IMAGE:-rasmol-linux-build}

echo "==> building image $image"
docker build -t "$image" -f "$here/scripts/Dockerfile.linux" "$here/scripts"

echo "==> building rasmol"
# --user keeps the staged binary owned by the invoking user rather than root.
docker run --rm \
    -v "$here":/src -w /src \
    --user "$(id -u):$(id -g)" \
    "$image" \
    -c 'scripts/build-linux.sh'

out="$here/binaries/linux-x86_64"
echo "==> $out/rasmol"
docker run --rm -v "$here":/src -w /src "$image" -c \
    'printf "    glibc floor: %s\n" \
        "$(objdump -T binaries/linux-x86_64/rasmol \
           | grep -oE "GLIBC_[0-9.]+" | sort -V | tail -1)"'
