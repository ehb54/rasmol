# RasMol — modernized fork (UltraScan SOMO)

A modernized build of the classic **RasMol** molecular graphics program, ported
off X11 to a single portable **SDL3 + Dear ImGui** frontend so it builds and runs
on current macOS, Linux, and Windows. This fork is maintained for use with the
**UltraScan SOMO** package.

RasMol's hand-optimized **CPU software renderer** — the reason it is still fast
and dependency-light decades later — is preserved unchanged. Only the platform/
windowing layer and build system were modernized.

Based on RasMol 2.7.6 by Roger Sayle and Herbert J. Bernstein. See
[Attribution & license](#attribution--license).

---

## What's new in this fork

- **SDL3 windowing frontend** (`src/sdl/`) replacing the old X11 layer — one code
  path for macOS, Linux, and Windows. X11 is no longer required.
- **CMake build system** replacing the legacy Imake/hand-Makefiles.
- **Pull-down menus and an in-window console** (Dear ImGui): File, Display,
  Colours, Options, Settings, Export, View, Help. Menu items and the console
  input run real RasMol commands, and the menus check-mark live state — the
  Display representations are mutually exclusive, as in classic RasMol.
- **Export menu** covering 13 output formats (PPM, PostScript, Vector PS, PICT,
  IRIS RGB, Sun Raster, POVRay 3, VRML, Kinemage, Raster3D, Molscript,
  Ramachandran, and RasMol script).
- **Atom picking** — click an atom to print its identity in the console (via
  RasMol's native mouse interface, so rotation/translation/zoom behave as in
  classic RasMol).
- **A fully featured console** — command history (up/down, persisted across
  sessions in `~/.rasmol_history`), Tab completion including context-aware
  colour/set/select arguments, selectable and copyable output, `Ctrl+L` to
  clear, `Ctrl-D` to exit.
- **A classic terminal `RasMol>` prompt** still works when launched from a shell,
  in addition to the in-window console.
- **SOMO bead-model support** — `rasmol -script model.spt` renders the bead
  models written by UltraScan SOMO, alongside ordinary PDB input.
- **CIF / mmCIF input** via RasMol's own built-in parser (no external library).
- **Help → About** reporting the fork version and exact build revision.
- **Large bead models load ~15× faster.** RasMol rebuilt every bond's selection
  state after each selection command; SOMO emits one selection per bead, and the
  XYZ reader distance-bonds packed beads heavily, so a 20,640-bead model spent
  most of two and a half minutes maintaining flags for bonds that are never
  drawn. That state is now derived on demand (158s → 10s).
- Numerous correctness fixes surfaced by modern toolchains (64-bit pointer
  truncation, uninitialized message tables, a multi-molecule load crash, etc.).

The self-contained software renderer and full RasMol command language are intact.

---

## Building

### Prerequisites

- **CMake** ≥ 3.16
- A **C and C++17** compiler (Apple Clang, GCC, or MSVC)
- **SDL3** (for the GUI frontend)

Install on macOS (Homebrew):

```sh
brew install cmake sdl3
```

On Windows, use the SDL3 SDK with MSVC.

**Linux:** SDL3 is new and often not yet packaged (e.g. it is absent from
EPEL 8 / Rocky 8). Build a lean static SDL3 from source into a user prefix —
only the X11 dev headers are needed as system packages:

```sh
# Build dependencies (RHEL/Rocky; use apt equivalents on Debian/Ubuntu):
sudo dnf install libX11-devel libXext-devel libXcursor-devel libXi-devel \
    libXfixes-devel libXrandr-devel libXrender-devel libxkbcommon-devel \
    mesa-libGL-devel mesa-libEGL-devel \
    wayland-devel wayland-protocols-devel   # for the native Wayland backend

# Build a minimal static SDL3 (video only) into ~/sdl3-prefix:
curl -LO https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz
tar xzf SDL3-3.4.12.tar.gz && cd SDL3-3.4.12
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/sdl3-prefix \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF \
    -DSDL_AUDIO=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF \
    -DSDL_CAMERA=OFF -DSDL_SENSOR=OFF -DSDL_X11_XTEST=OFF -DSDL_X11_XSCRNSAVER=OFF
cmake --build build -j && cmake --install build
cd ..
```

On Rocky/RHEL 8 the base GCC is too old; use a newer toolchain, e.g.
`scl enable gcc-toolset-13 '<build commands>'`.

### Configure & build

```sh
# Point CMake at your SDL3. macOS (Homebrew):
export CMAKE_PREFIX_PATH="$(brew --prefix sdl3)"
# Linux (SDL3 built above):
export CMAKE_PREFIX_PATH="$HOME/sdl3-prefix"

cmake -S . -B build
cmake --build build -j4
```

On Linux the GUI links SDL3, ImGui, and the C++/GCC runtime statically; the
resulting binary depends only on core glibc (`libc`, `libm`, `libpthread`,
`libdl`) and loads its windowing backend dynamically at runtime.

A single binary supports **X11, Wayland, and KMSDRM** — SDL3 compiles all
enabled backends in and selects one at runtime (honoring `SDL_VIDEODRIVER`), so
no per-session build is needed. `libdecor` (client-side Wayland window
decorations) is optional and not packaged on RHEL/Rocky 8; without it, windows
on compositors that require client-side decorations (e.g. GNOME Wayland) appear
borderless — run with `SDL_VIDEODRIVER=x11` there to use XWayland instead.

This produces two executables in `build/`:

| Target        | Description                                             |
|---------------|---------------------------------------------------------|
| `rasmol`      | The SDL3 GUI (menus, in-window console, mouse control). |
| `rasmol-text` | A headless/text build (scripting, batch rendering).     |

### CMake options

| Option                | Default | Effect                                                        |
|-----------------------|---------|---------------------------------------------------------------|
| `RASMOL_BUILD_SDL`    | `ON`    | Build the SDL3 GUI (`rasmol`). Requires SDL3.                  |
| `RASMOL_BUILD_TEXT`   | `ON`    | Build the headless text frontend (`rasmol-text`).             |
| `RASMOL_USE_CBFLIB`   | `OFF`   | CBF binary / CBF-map support via CBFlib. **Not yet wired up.** |

### One-shot distribution builds

The `scripts/` directory reproduces the exact binaries in `binaries/` — each
script builds a lean static SDL3 from source, builds RasMol against it, strips
the result, and stages it under `binaries/<platform>/`:

```sh
scripts/build-macos.sh      # -> binaries/macos-universal/  (arm64 + x86_64, run on macOS)
scripts/build-linux.sh      # -> binaries/linux-x86_64/     (run on Linux)
scripts/build-windows.sh    # -> binaries/windows-x86_64/   (MinGW cross-compile, run on macOS or Linux)
```

Notes:
- **Linux:** for the widest reach, run on the oldest glibc you want to support
  (a binary built against old glibc runs on that and every newer version). If
  the system compiler is too old for C++17, prefix a newer one, e.g.
  `scl enable gcc-toolset-13 scripts/build-linux.sh` (Rocky/RHEL 8) or
  `CC=gcc-9 CXX=g++-9 scripts/build-linux.sh` (Ubuntu 16.04).
- **Windows:** needs `mingw-w64` (`brew install mingw-w64`, or the distro's
  `g++-mingw-w64-x86-64` / `mingw64-gcc-c++`). Uses
  `cmake/mingw-toolchain.cmake`.

SDL3 sources are cached under `build-deps/` (git-ignored); delete a
`build-deps/sdl3-*` prefix to force an SDL3 rebuild.

---

## Running

### GUI

```sh
./build/rasmol data/1crn.pdb            # PDB
./build/rasmol data/4ins.CIF            # CIF/mmCIF (auto-detected by extension)
./build/rasmol -script model.spt        # run a RasMol script at startup
./build/rasmol -snapshot shot.bmp ...   # render, write a BMP, and exit
```

`-script` is how **UltraScan SOMO** launches bead models: it writes a `.bms`
(XYZ coordinates) plus a `.spt` script that sizes and colours each bead, and
invokes `rasmol -script <model>.spt`.

- **Mouse:** left-drag rotates, Shift+left-drag or right-drag translates, the
  wheel zooms.
- **Pick an atom:** click it — its identity prints in the console
  (`Atom: CA 41  Group: ILE 7`). Change what a click does from the **Settings**
  menu (Ident / Distance / Angle / …).
- **Commands:** type any RasMol command in the docked console (e.g. `spacefill`,
  `colour chain`, `cartoons on`, `select :A`). If you launched from a terminal,
  the classic `RasMol>` prompt there works too.
- **Menus** cover the common representation, colour, and option commands.

### Headless / scripting

```sh
# Drive the text build with a RasMol script on stdin:
printf 'load pdb "data/1crn.pdb"\nspacefill\ncolour cpk\nwrite ppm out.ppm\nexit\n' \
  | ./build/rasmol-text
```

Supported input formats include PDB, mmCIF/CIF, MDL mol, Sybyl mol2, XYZ, MOPAC,
Alchemy, and CHARMm (see the `load` help topic). Up to 20 molecules may be loaded
at once and switched with `molecule <n>`.

---

## Project layout

```
CMakeLists.txt        Top-level build
src/                  RasMol core (renderer, molecule model, command engine, I/O)
  sdl/                SDL3 + ImGui frontend (sdlwin.c, ui_imgui.cpp, ui.h)
  rastxt.c            Headless text frontend
third_party/          Vendored libraries (self-contained build)
  CVector, NearTree, CQRlib   Bernstein/Andrews support libs (LGPL)
  imgui               Dear ImGui + SDL3 backends (MIT)
data/                 Sample structures
doc/                  RasMol user documentation (help file, manual)
claude/               Developer research notes
```

---

## Status & known limitations

- **macOS** and **Linux** (Rocky Linux 8.10 / GCC 13 and Ubuntu 16.04, X11 and
  VNC) are validated — the GUI builds and renders identically on both, driven
  from SOMO for PDBs and bead models, and the binaries are statically linked for
  distribution. **Windows** cross-compiles to a self-contained static
  `rasmol.exe` with MinGW-w64 (provided under `binaries/`) and is validated on
  **Windows 10** — GUI, PDB and bead-model loading all behave as on the other
  platforms. Older Windows is untested; see the UCRT note below.
- **Windows 7 is untested but has no known blocker.** The exe declares an
  XP-era subsystem version and imports no post-Win7 APIs, and SDL3 supports
  desktop Windows back to XP. It does import the **UCRT**
  (`api-ms-win-crt-*`), which is built into Windows 10 but reaches Windows 7
  SP1 only via update **KB2999226** — present on most patched Win7 machines.
  If it is missing, the failure is an explicit missing-DLL dialog naming
  `api-ms-win-crt-runtime-l1-1-0.dll`.
- Displays that advertise GLX but cannot create an OpenGL context — notably some
  older VNC servers — are handled by falling back to SDL's software renderer,
  with a note on stderr. RasMol rasterizes on the CPU, so nothing is lost.
- **`-nodisplay` still requires a video device on Linux**; use the `rasmol-text`
  build for genuinely headless scripting.
- **Display → Molecular Surface draws a dot surface.** RasMol's solid (`surface`)
  renderer is broken upstream — it renders nothing here and crashes 2.7.5.2 — so
  the menu issues `dots` instead.
- **BMP and GIF export are not offered.** RasMol's writers for those two formats
  are 8-bit only and this is a 32-bit colour build; the other 13 export formats
  work (PPM is the raster one).
- **CBF binary format** (imgCIF) and **CBF electron-density maps** are not
  supported — those require CBFlib (`RASMOL_USE_CBFLIB`, not yet wired). Ordinary
  CIF/mmCIF **coordinate** files work via the built-in parser.
- The `align` (structure superposition) command is **undocumented and
  unfinished** in upstream RasMol and does not produce correct results; it is not
  part of the supported feature set. See
  [`claude/align-research.md`](claude/align-research.md) for a full analysis.

---

## Attribution & license

RasMol was originally written by **Roger Sayle** (Glaxo Wellcome, 1992–1999). The
2.7.x "OpenRasMol" line was developed by **Herbert J. Bernstein** (1998–2011).
This fork modernizes the build and frontend; the molecular engine is RasMol's.

RasMol is **dual-licensed** under the GNU General Public License (see [`GPL`](GPL))
or the RasMol license RASLIC (see [`RASLIC`](RASLIC)); consult [`NOTICE`](NOTICE)
for the complete terms and the full list of contributors.

Vendored libraries retain their own licenses: CVector, NearTree, and CQRlib
(LGPL, H. J. Bernstein & L. C. Andrews) under `third_party/`, and Dear ImGui
(MIT) under `third_party/imgui/`.

Upstream / original project: <https://www.openrasmol.org/>
