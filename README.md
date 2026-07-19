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
  Colours, Options, Settings, View, Help. Menu items and the console input run
  real RasMol commands.
- **Atom picking** — click an atom to print its identity in the console (via
  RasMol's native mouse interface, so rotation/translation/zoom behave as in
  classic RasMol).
- **A classic terminal `RasMol>` prompt** still works when launched from a shell,
  in addition to the in-window console.
- **CIF / mmCIF input** via RasMol's own built-in parser (no external library).
- **Help → About** reporting the fork version and exact build revision.
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
    mesa-libGL-devel mesa-libEGL-devel

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
`libdl`) and loads X11/GL dynamically at runtime.

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

---

## Running

### GUI

```sh
./build/rasmol data/1crn.pdb        # PDB
./build/rasmol data/4ins.CIF        # CIF/mmCIF (auto-detected by extension)
```

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

- **macOS** and **Linux** (Rocky Linux 8.10, GCC 13, X11) are validated — the
  GUI builds and renders identically on both, and the Linux binary is
  statically linked for distribution. **Windows** shares the same CMake + SDL3
  path and is expected to build but has **not yet been validated**.
- **CBF binary format** (imgCIF) and **CBF electron-density maps** are not
  supported — those require CBFlib (`RASMOL_USE_CBFLIB`, not yet wired). Ordinary
  CIF/mmCIF **coordinate** files work via the built-in parser.
- The `align` (structure superposition) command is **undocumented and
  unfinished** in upstream RasMol and does not produce correct results; it is not
  part of the supported feature set. See
  [`claude/align-research.md`](claude/align-research.md) for a full analysis.
- Some menu **Options** toggles do not yet reflect live state (checkmarks).

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
