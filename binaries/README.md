# Precompiled RasMol binaries

Ready-to-run builds of the SDL3 GUI (`rasmol`). Each folder contains the
executable and its help file — **keep `rasmol.hlp` next to `rasmol`** (the
program looks for it in its own directory).

| Folder | Architecture | Runs on |
|--------|--------------|---------|
| `linux-x86_64/`    | x86-64            | Any glibc ≥ 2.17 Linux — CentOS/RHEL 7, Ubuntu 14.04, and everything newer (built on Ubuntu 16.04) |
| `macos-universal/` | Intel + Apple Silicon | macOS 11 (Big Sur) and later |
| `windows-x86_64/`  | x86-64            | Windows 10 and later — **built (MinGW), not yet validated on real Windows** |

These are statically linked (SDL3, Dear ImGui, and the C++ runtime are baked
in). The Linux binary depends only on core glibc and loads X11/Wayland/OpenGL
at runtime; the macOS binary depends only on system frameworks. No separate
install or libraries are required.

## Running

```sh
cd linux-x86_64      # or macos-universal
./rasmol structure.pdb          # PDB
./rasmol structure.cif          # CIF/mmCIF
```

Left-drag rotates, the wheel zooms, Shift/right-drag translates, and clicking an
atom identifies it in the console. Type RasMol commands in the docked console.
See the repository [README](../README.md) for full usage.

### macOS note

`rasmol` is ad-hoc signed (both the arm64 and x86_64 slices), so it runs on
Apple Silicon and Intel without any extra step. An ad-hoc signature is not a
Developer ID signature, though, so macOS Gatekeeper may still block the first
launch if you obtained the binary via a downloaded archive. Either right-click →
**Open** once, or clear the quarantine flag:

```sh
xattr -dr com.apple.quarantine ./rasmol
```

### Windows note

`rasmol.exe` is a self-contained static build (cross-compiled with MinGW-w64);
it needs only the Windows system DLLs and the Universal CRT present on Windows
10+. It has **not yet been validated on real Windows** — please report whether
it runs. A console window opens alongside the graphics window for command
output. Run it from a folder that also contains `rasmol.hlp`:

```
rasmol.exe structure.pdb
```

## Rebuilding

These are produced from the source in this repository — see the top-level
[README](../README.md) for build instructions. For maximum Linux compatibility,
build on the oldest glibc you want to support (a binary built against old glibc
runs on that and every newer version).
