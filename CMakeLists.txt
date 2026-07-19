cmake_minimum_required(VERSION 3.16)

project(RasMol
    VERSION 2.7.6.0
    DESCRIPTION "RasMol molecular graphics -- modernized fork for UltraScan SOMO"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pin the C dialect to gnu11: the legacy K&R-style sources rely on pre-C23
# semantics (notably empty () meaning an unprototyped function). Newer GCC
# (15+) defaults to C23 and would reject them. (C11 rather than C17 so this
# also works on older CMake, e.g. 3.20 — CMAKE_C_STANDARD 17 needs CMake 3.21.)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(RASMOL_USE_CBFLIB "Build with CBFlib for CIF/CBF and map I/O" OFF)
# The text frontend's interactive console uses POSIX termios; default it off on
# Windows (the SDL GUI has its own in-window console).
if(WIN32)
    set(_rasmol_text_default OFF)
else()
    set(_rasmol_text_default ON)
endif()
option(RASMOL_BUILD_TEXT "Build the headless text frontend (rasmol-text)" ${_rasmol_text_default})

set(RASMOL_DATADIR "${CMAKE_INSTALL_PREFIX}/share/rasmol"
    CACHE PATH "Runtime data directory (help text, rasmolrc, etc.)")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Tolerances for the legacy C sources.
# The code predates C99 strictness; recent clang/gcc promote implicit-int and
# implicit-function-declaration to errors. Keep them as warnings so the port
# builds across toolchains while we modernize incrementally.
# ---------------------------------------------------------------------------
include(CheckCCompilerFlag)
set(RASMOL_LEGACY_FLAGS "")
foreach(flag -Wno-implicit-function-declaration -Wno-implicit-int
             -Wno-int-conversion -Wno-deprecated-non-prototype)
    string(MAKE_C_IDENTIFIER "HAVE${flag}" flagvar)
    check_c_compiler_flag("${flag}" ${flagvar})
    if(${flagvar})
        list(APPEND RASMOL_LEGACY_FLAGS ${flag})
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Vendored support libraries (see third_party/README.md)
# ---------------------------------------------------------------------------
add_library(rasmol_extlib STATIC
    third_party/CVector/CVector.c
    third_party/NearTree/CNearTree.c
    third_party/CQRlib/cqrlib.c)
target_include_directories(rasmol_extlib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CVector
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/NearTree
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CQRlib)
target_compile_options(rasmol_extlib PRIVATE ${RASMOL_LEGACY_FLAGS})

# ---------------------------------------------------------------------------
# RasMol core: renderer, molecule model, command engine, file I/O.
# Platform/windowing code is deliberately excluded -- frontends add their own.
# ---------------------------------------------------------------------------
set(RASMOL_CORE_SOURCES
    src/molecule.c
    src/abstree.c
    src/cmndline.c
    src/command.c
    src/transfor.c
    src/render.c
    src/repres.c
    src/pixutils.c
    src/outfile.c
    src/script.c
    src/infile.c
    src/tokens.c
    src/multiple.c
    src/vector.c
    src/wbrotate.c
    src/langsel_unix.c
    src/maps.c
    src/cif_fract.c
    src/CVector_3D.c
    src/rmsd.c)

add_library(rasmol_core STATIC ${RASMOL_CORE_SOURCES})
target_include_directories(rasmol_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rasmol_core PUBLIC rasmol_extlib)
target_compile_options(rasmol_core PRIVATE ${RASMOL_LEGACY_FLAGS})
target_compile_definitions(rasmol_core PUBLIC
    THIRTYTWOBIT
    "RASMOLDIR=\"${RASMOL_DATADIR}\"")

if(RASMOL_USE_CBFLIB)
    # TODO: locate and link CBFlib + build cif.c/cif_stx.c/cif_ctonum.c with
    # USE_CBFLIB for CBF binary format and CBF map support.
    message(FATAL_ERROR "RASMOL_USE_CBFLIB is not wired up yet")
else()
    target_compile_definitions(rasmol_core PUBLIC NO_CBFLIB)
    # RasMol's own CIF parser (no external library) reads CIF/mmCIF
    # coordinate files. It provides the cif_* layer infile.c calls.
    target_sources(rasmol_core PRIVATE
        src/cif.c
        src/cif_stx.c
        src/cif_ctonum.c)
endif()

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(rasmol_core PUBLIC ${MATH_LIBRARY})
endif()

# ---------------------------------------------------------------------------
# Headless text frontend
# ---------------------------------------------------------------------------
if(RASMOL_BUILD_TEXT)
    add_executable(rasmol-text
        src/rastxt.c
        src/rasmol_text_stubs.c)
    target_link_libraries(rasmol-text PRIVATE rasmol_core)
    target_compile_options(rasmol-text PRIVATE ${RASMOL_LEGACY_FLAGS})
    install(TARGETS rasmol-text RUNTIME DESTINATION bin)
endif()

# ---------------------------------------------------------------------------
# SDL3 GUI frontend -- the portable replacement for the X11 platform layer
# ---------------------------------------------------------------------------
option(RASMOL_BUILD_SDL "Build the SDL3 GUI frontend (rasmol)" ON)

set(RASMOL_HAVE_SDL OFF)
if(RASMOL_BUILD_SDL)
    find_package(SDL3 QUIET CONFIG)
    if(SDL3_FOUND)
        set(RASMOL_HAVE_SDL ON)

        # Dear ImGui (vendored) built as a static library, incl. SDL3 backends.
        add_library(rasmol_imgui STATIC
            third_party/imgui/imgui.cpp
            third_party/imgui/imgui_draw.cpp
            third_party/imgui/imgui_tables.cpp
            third_party/imgui/imgui_widgets.cpp
            third_party/imgui/backends/imgui_impl_sdl3.cpp
            third_party/imgui/backends/imgui_impl_sdlrenderer3.cpp)
        target_include_directories(rasmol_imgui PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
        target_link_libraries(rasmol_imgui PUBLIC SDL3::SDL3)

        # Capture the git revision for the About box (best-effort).
        execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE RASMOL_GIT_REV
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(NOT RASMOL_GIT_REV)
            set(RASMOL_GIT_REV "unknown")
        endif()

        add_executable(rasmol
            src/sdl/sdlwin.c
            src/sdl/ui_imgui.cpp)
        target_include_directories(rasmol PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl)
        target_link_libraries(rasmol PRIVATE rasmol_core rasmol_imgui SDL3::SDL3)
        target_compile_options(rasmol PRIVATE ${RASMOL_LEGACY_FLAGS})
        target_compile_definitions(rasmol PRIVATE
            "RASMOL_GIT_REV=\"${RASMOL_GIT_REV}\"")

        # For distribution, statically link the C++/GCC runtime on GCC (Linux)
        # so the binary does not depend on the target system's libstdc++
        # version. SDL3/ImGui are already static libraries; X11/GL are loaded
        # dynamically by SDL at runtime.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(rasmol PRIVATE -static-libgcc -static-libstdc++)
        endif()

        # Ship the help file next to the binary so RASMOLPATH (set to the
        # executable's directory at runtime) finds it in dev builds.
        if(EXISTS ${CMAKE_SOURCE_DIR}/doc/rasmol.hlp)
            add_custom_command(TARGET rasmol POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${CMAKE_SOURCE_DIR}/doc/rasmol.hlp
                        $<TARGET_FILE_DIR:rasmol>/rasmol.hlp)
            install(FILES ${CMAKE_SOURCE_DIR}/doc/rasmol.hlp DESTINATION bin)
        endif()
        install(TARGETS rasmol RUNTIME DESTINATION bin)
    else()
        message(WARNING
            "SDL3 not found -- skipping the GUI frontend. "
            "Install SDL3 (e.g. `brew install sdl3`) and reconfigure.")
    endif()
endif()

message(STATUS "RasMol ${PROJECT_VERSION}")
message(STATUS "  CBFlib (CIF/CBF I/O) : ${RASMOL_USE_CBFLIB}")
message(STATUS "  text frontend        : ${RASMOL_BUILD_TEXT}")
message(STATUS "  SDL3 GUI frontend    : ${RASMOL_HAVE_SDL}")
message(STATUS "  data dir (RASMOLDIR)  : ${RASMOL_DATADIR}")
