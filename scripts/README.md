# Release build scripts

These produce the binaries committed under [`binaries/`](../binaries). They are
for cutting a release; to build RasMol for your own machine, follow **Building**
in the [top-level README](../README.md) instead.

Each script stages its result straight into `binaries/<platform>/` and prints
what it produced, so a release is: run the script, check the reported floor or
signature, commit the binary.

| Script | Produces | Run on |
|--------|----------|--------|
| `build-macos.sh` | `binaries/macos-universal/rasmol` (arm64 + x86_64) | macOS |
| `build-linux-docker.sh` | `binaries/linux-x86_64/rasmol` | anywhere with Docker |
| `build-linux.sh` | same, without the container | a glibc 2.17 host (see below) |
| `build-windows.sh` | `binaries/windows-x86_64/rasmol.exe` | macOS or Linux, with mingw-w64 |
| `build-sdl3.sh` | a private static SDL3 under `build-deps/` | called by the others |

None of them need SDL3 installed on the host — `build-sdl3.sh` builds a static
one into `build-deps/` first, so the shipped binaries carry SDL3 inside them and
have no SDL runtime dependency.

## Linux — the glibc floor

**The Linux binary targets glibc 2.17 (CentOS/RHEL 7 era) and this is
deliberate.** A binary inherits the floor of the glibc it was linked against and
will not start on anything older, so the floor decides which systems the release
runs on. Scientific installations routinely run old distributions; keeping the
floor low costs nothing and dropping it strands users.

The floor is pinned by the base image in [`Dockerfile.linux`](Dockerfile.linux),
which is the only place it should be changed — and changing it is a
compatibility decision, not a version bump. Update `binaries/README.md` to match
if you ever do.

```sh
scripts/build-linux-docker.sh
```

`build-linux.sh` is the underlying build and can still be run directly on a
suitable host, but then the floor is whatever that host happens to have. It
needs cmake, the X11 development headers, and a C++17 compiler — on a
distribution old enough to give a low floor, the system compiler usually is not
new enough, hence `CC=gcc-9 CXX=g++-9` or `scl enable gcc-toolset-13`. The
container removes all of that guesswork; prefer it.

## macOS

```sh
scripts/build-macos.sh
```

Builds both architectures, strips, then **ad-hoc signs after stripping** —
stripping rewrites the Mach-O and does not preserve the linker's signature. The
script verifies the result; note that `codesign` inspects only the slice
matching the current machine unless you pass `--arch`, so a fat binary can look
signed while the other slice is not:

```sh
codesign --verify --strict --arch arm64  binaries/macos-universal/rasmol
codesign --verify --strict --arch x86_64 binaries/macos-universal/rasmol
```

## Windows

Cross-compiled, so no Windows machine is needed to build:

```sh
brew install mingw-w64        # macOS; apt install g++-mingw-w64-x86-64 on Debian
scripts/build-windows.sh
```

## Verifying a build without a GUI

`rasmol -nodisplay` runs a script and exits, needing no display server — useful
for checking a build over ssh or in CI:

```sh
printf 'load structure.pdb\nselect 10:A\nquit\n' | ./rasmol -nodisplay
```

Two things to know before trusting the output:

- **Commands run from a `-script` file are quiet.** `DisplaySelectCount()` only
  reports when `FileDepth == -1`, so informational lines such as
  `No atoms selected!` appear when commands arrive on **stdin**, and not when
  the same commands come from `-script`. A silent `-script` run proves nothing.
  This is upstream behaviour and applies to every platform.
- **`write` is refused inside a script** unless you pass `-insecure`, which sets
  `AllowWrite`. That is what makes the rendering check below possible.

To compare a rendering across platforms, have the script write an image and
diff the pixels:

```sh
printf 'load structure.pdb\nspacefill 300\nwrite ppm out.ppm\nquit\n' > check.spt
./rasmol -nodisplay -insecure -script check.spt
```

Compare the **pixel payload, not the whole file**: the PPM header ends `\r\n` on
Windows because the image is opened in text mode, making the file one byte
longer than the Unix equivalent. Skip 15 bytes on Unix and 16 on Windows.
