# RasMol modernization — session handoff

Working notes for continuing this fork. Written 2026-07-19.

## What this is

A modernized fork of **RasMol** for the **UltraScan SOMO** package. Goal: get
RasMol off X11 onto a single portable **SDL3 + Dear ImGui** frontend that builds
and runs on current macOS, Linux, and Windows, while **preserving RasMol's CPU
software renderer** (the reason it outperforms other viewers, per the user).

- **Repo:** <https://github.com/ehb54/rasmol>
- **Default branch:** `somo-modernize` (based on the `RasMol-2.7.6.0` tag).
  `master` is untouched upstream (Bernstein's later, unfinished align line —
  see §"align").
- **Local checkout:** `~/claude/rasmol` (on the user's Mac).
- **Commit author:** set `git config user.email "emre.brookes@umontana.edu"`
  before committing. End commit messages with the Co-Authored-By trailer.
- **macOS and Linux are validated; Windows builds but is unverified at runtime.**

## Architecture (how the port works)

RasMol's core rasterizes into `FBuffer` (a 32-bit `0x00RRGGBB` array in the
`-DTHIRTYTWOBIT` build). The frontend just uploads that to an SDL texture — the
renderer is untouched.

- **`src/sdl/sdlwin.c`** (C) — the SDL frontend: owns the window/event loop,
  implements RasMol's ~25-function graphics contract (`graphics.h`), the
  terminal console (POSIX raw mode, `#ifndef _WIN32`), and the C callbacks the
  UI uses. A frontend must `#define RASMOL` + `#define GRAPHICS` before the
  RasMol includes to emit the core globals, and define a handful of
  frontend-owned symbols (see the file).
- **`src/sdl/ui_imgui.cpp`** (C++) — pure UI: menu bar, docked console,
  About. Never includes RasMol's C headers; talks to the core only through the
  callbacks in **`src/sdl/ui.h`** (`run_command`, `console_text`, `get_state`,
  `save_as`, `print`, `clear`, `open_file`, `quit`, `about`).
- **`src/rastxt.c`** — the headless text frontend (`rasmol-text`).
- Mouse uses RasMol's native `ProcessMouseDown/Move/Up` (rotation + atom
  picking). Menus/console input turn into RasMol command strings run via
  `RunCommandString` (feeds `ProcessCharacter`/`ExecuteCommand`).

## Build system

CMake (`CMakeLists.txt`) — replaces the old Imake. Targets: `rasmol` (SDL GUI),
`rasmol-text` (headless). Options: `RASMOL_BUILD_SDL`, `RASMOL_BUILD_TEXT`
(off on Windows), `RASMOL_USE_CBFLIB` (OFF, unwired). C dialect pinned to
**gnu11** (`CMAKE_C_STANDARD 11` — 17 needs CMake ≥3.21; gnu11 still fixes the
GCC-15+/C23 empty-`()` issue). Static C++/GCC runtime on Linux via
`-static-libgcc -static-libstdc++` (guarded to GNU).

**Vendored** under `third_party/`: CVector 1.0.3, NearTree 5.1, CQRlib 1.0.3
(Bernstein/Andrews, LGPL); Dear ImGui 1.92.9 (MIT, + SDL3 backends). SDL3 is
**not** vendored — built from source per platform (see scripts).

### One-shot build scripts (`scripts/`) — reproduce `binaries/`

- `scripts/build-macos.sh` → `binaries/macos-universal/` (arm64+x86_64, macOS 11).
- `scripts/build-linux.sh` → `binaries/linux-x86_64/`. Honors `CC`/`CXX`; on
  old-GCC systems prefix `scl enable gcc-toolset-13 ...` or `CC=gcc-9 CXX=g++-9`.
- `scripts/build-windows.sh` → `binaries/windows-x86_64/` (MinGW cross-compile;
  needs `brew install mingw-w64`; uses `cmake/mingw-toolchain.cmake`).
- `scripts/build-sdl3.sh` — shared: fetches + builds lean static SDL3 (video
  only) into `build-deps/` (git-ignored). Delete a `build-deps/sdl3-*` prefix to
  force an SDL3 rebuild.

Each script builds static SDL3, builds RasMol, strips, and stages the binary +
`rasmol.hlp` into `binaries/<platform>/`. **Commit binaries from the Mac**, not
from the build hosts. Large binary pushes need `git config http.postBuffer
524288000` (avoids sideband disconnect).

## Remote build/test machines

- **`ssh mao`** — Ubuntu 16.04.7 (glibc 2.23), work in `~/claude`. Use
  `CC=gcc-9 CXX=g++-9` (default gcc-5 too old); cmake 3.20. SDL3 prebuilt at
  `~/claude/sdl3-prefix`. **This is where the distributed Linux binary is
  built** — building on old glibc gives a **GLIBC_2.17 floor**, so one binary
  runs on RHEL/CentOS 7, Ubuntu 14.04+, and everything newer. Linux rebuild:
  `ssh mao 'cd ~/claude/rasmol && git pull -q origin somo-modernize && CC=gcc-9
  CXX=g++-9 cmake --build build-linux -j$(nproc) && cp build-linux/rasmol
  /tmp/rl && strip /tmp/rl'` then `scp mao:/tmp/rl
  binaries/linux-x86_64/rasmol`.
- **`ssh uslimstest`** — Rocky Linux 8.10 (glibc 2.28), VNC on `DISPLAY=:5` the
  user tests with. Use `scl enable gcc-toolset-13`. Was used for the first Linux
  validation (interactive GUI confirmed working on the VNC).
- git clone on these boxes occasionally hits a transient GitHub SSL error —
  just retry.

## Distributable = executable + `rasmol.hlp`

The binary sets `RASMOLPATH` to its own directory, so `rasmol.hlp` must sit
next to the executable. Everything else (SDL3, ImGui, C++ runtime) is static;
Linux is glibc-only + dlopen'd X11/Wayland/GL; macOS is system frameworks only
(no Homebrew); Windows is system DLLs + UCRT (Win10+). One static binary per
(OS, arch) — glibc is forward-compatible, so no per-distro builds.

## Feature state

Working: PDB + CIF/mmCIF input (built-in parser, no CBFlib); SOMO **bead
models** (`rasmol -script model.spt` — the `-script` flag was the fix); full
menu bar matching RasMol 2.7.5.2 (File/Display/Colours/Options/Settings/Export/
View/Help) with check marks; **exclusive Display** representations; Options
**Hydrogens/Hetero** toggles; atom picking → console; in-window **console**
(selectable/copyable, auto-scroll, history up/down + persistent
`~/.rasmol_history`, Tab completion incl. context-aware colour/set/select args,
Ctrl+L clear, Ctrl-D exit); terminal `RasMol>` prompt too; Help→About with git
revision.

## Open issues / TODO

1. **Windows 7 pending.** Windows 10 is **validated** (2026-07-20, user's VM:
   GUI, PDBs and bead models all fine, bead load fast). A colleague is testing
   Windows 7 bare metal. Static analysis of `rasmol.exe` found no blocker —
   XP-era subsystem version, no post-Win7 imports, SDL3 supports desktop
   Windows back to XP — but it imports the UCRT (`api-ms-win-crt-*`), which on
   Win7 SP1 requires update KB2999226. If that is the failure, the symptom is a
   missing-DLL dialog for `api-ms-win-crt-runtime-l1-1-0.dll`; the fallback
   would be relinking against `msvcrt.dll` (the MinGW toolchain ships
   `libmsvcrt.a`), which is not a flag flip and needs testing.

   **File→Open verification pending on the new build.** Win7 (and macOS)
   originally hung after picking a file: SDL's native file dialog runs its
   callback on a worker thread on Windows (`SDL_CreateThread`), and the
   callback ran the load — RasMol command engine + SDL renderer, neither
   thread safe — directly, blanking the window. Fixed (commit ebf66dc,
   binaries 71b7126) by marshalling dialog commands to the main loop via a
   registered SDL event; verify Open/Save As in the Win10 VM. **Rule for any
   new dialog callback: never touch the core or renderer in it — call
   `DeferCommand`.** Same commit fixed a POSIX signal-handler double-free on
   exit (`ConsoleSignal` now only sets a flag; verified with a SIGTERM test).
2. **Console interactive behaviors need user verification** — auto-scroll,
   Ctrl-D/Ctrl+L, Tab, cross-session history. All compile + render; couldn't be
   driven in headless snapshots. Auto-scroll uses an outer-child +
   content-sized `InputTextMultiline` + `SetScrollHereY`; if it still doesn't
   follow output, that's the thing to revisit.
3. **`surface` (solid molecular surface) is broken in RasMol** — renders
   nothing here, crashes 2.7.5.2. Display→Molecular Surface uses the **dot**
   surface (`dots`) instead. A real solid/Connolly surface would be a RasMol
   renderer fix.
4. **Export: BMP/GIF dropped** — RasMol's BMP/GIF writers are 8-bit only and
   this is a 32-bit build. 13 formats work (PPM is the raster one). Could add
   truecolor BMP (route via `SDL_SaveBMP`) if wanted.
5. **Hydrogens/Hetero "show"** re-applies wireframe (RasMol's `restrict` is
   destructive of representation). Fine for wireframe views; if user wants it to
   preserve the current representation, track last Display choice and re-apply.
6. **`align` command** — undocumented, unfinished (blank/wrong results),
   depends on a private `CQRHLERP` only in Bernstein's unreleased CQRlib (maybe
   on the user's old server / an unreleased 2.8.x). Full analysis in
   [`align-research.md`](align-research.md). Multi-molecule loading itself was
   fixed (missing `InitialiseMultiple()`), so it's reachable but not functional.
   Not worth pursuing unless SOMO needs structure superposition.
7. **`(null)`-class latent bugs** — fixed several (CurPrompt, MsgStrs via
   `SwitchLang(English)`, 64-bit pointer truncation, VersionStr overflow); more
   may exist in less-traveled code paths.
8. **CBFlib / CBF binary + CBF maps** — unwired (`RASMOL_USE_CBFLIB` is a hard
   error). CBFlib 0.9.2 (no HDF5 dep) is the version RasMol targets; a tarball
   is at `~/claude/tmp/cbflib` if ever needed. Ordinary CIF works without it.

## SOMO integration notes

SOMO source: `~/claude/ultrascan3/us_somo/develop` (also `~/ultrascan3`,
`~/ultrascan`). It launches `rasmol <file>` for PDB and `rasmol -script
<file>.spt` for bead models (`us_hydrodyn.cpp` `model_viewer`;
`us_hydrodyn_write.cpp` `write_bead_spt` writes `load xyz` + per-bead `select
atomno=N; spacefill R; colour C`). A disabled movie path pipes script text to
rasmol stdin (`rasmol->launch`, under `#if TODO_FIX_MOVIE_FRAME`) — if enabled,
the SDL frontend would need to read a script from stdin.
