cmake_minimum_required(VERSION 3.16)

project(RasMol
    VERSION 2.7.6.0
    DESCRIPTION "RasMol molecular graphics -- modernized fork for UltraScan SOMO"
    LANGUAGES C)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(RASMOL_USE_CBFLIB "Build with CBFlib for CIF/CBF and map I/O" OFF)
option(RASMOL_BUILD_TEXT "Build the headless text frontend (rasmol-text)" ON)

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
             -Wno-deprecated-non-prototype)
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
    # TODO: locate and link CBFlib + its cif.c/cif_ctonum.c/cif_stx.c sources.
    message(FATAL_ERROR "RASMOL_USE_CBFLIB is not wired up yet")
else()
    target_compile_definitions(rasmol_core PUBLIC NO_CBFLIB)
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
# GUI frontend (SDL3) -- added in the next step
# ---------------------------------------------------------------------------
# add_subdirectory(src/sdl)   # provides the `rasmol` target

message(STATUS "RasMol ${PROJECT_VERSION}")
message(STATUS "  CBFlib (CIF/CBF I/O) : ${RASMOL_USE_CBFLIB}")
message(STATUS "  text frontend        : ${RASMOL_BUILD_TEXT}")
message(STATUS "  data dir (RASMOLDIR)  : ${RASMOL_DATADIR}")
