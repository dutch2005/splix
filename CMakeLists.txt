cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0048 NEW)

project(splix
    VERSION     2.0.2
    DESCRIPTION "Samsung / Xerox / Dell printer CUPS driver (QPDL)"
    LANGUAGES   CXX
)

# ────────────────────────────────────────────────────────────────────────────
# C++ Standard
# ────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS        OFF)   # -std=c++11, not -std=gnu++11

# ────────────────────────────────────────────────────────────────────────────
# Global Configuration Options
# ────────────────────────────────────────────────────────────────────────────
option(DISABLE_JBIG "Disable JBIG compression" OFF)
option(DISABLE_THREADS "Disable POSIX threads" OFF)
option(DISABLE_BLACKOPTIM "Disable black optimization" OFF)
set(THREADS "2" CACHE STRING "Number of threads to use")
set(CACHESIZE "30" CACHE STRING "Cache size in MB")

if(DISABLE_THREADS)
    add_compile_definitions(DISABLE_THREADS)
else()
    add_compile_definitions(THREADS=${THREADS} CACHESIZE=${CACHESIZE})
endif()

if(DISABLE_JBIG)
    add_compile_definitions(DISABLE_JBIG)
endif()

if(DISABLE_BLACKOPTIM)
    add_compile_definitions(DISABLE_BLACKOPTIM)
endif()

# ────────────────────────────────────────────────────────────────────────────
# Core Dependencies
# ────────────────────────────────────────────────────────────────────────────
# We use CMake's built-in `FindCups` module because it natively searches 
# the system paths for cups/cups.h and the libcups library. We specifically
# avoid `pkg_check_modules` because some older Debian configurations lack
# the cups.pc file but still provide the library and headers.

find_package(PkgConfig REQUIRED)

# FindCups provides CUPS_INCLUDE_DIRS and CUPS_LIBRARIES
find_package(Cups)
if(NOT CUPS_FOUND)
    message(FATAL_ERROR
        "\n[splix] CUPS development headers not found."
        "\nInstall them with:"
        "\n  apt-get install cups libcups2-dev\n"
    )
endif()

# libcupsimage — companion library shipped alongside CUPS but NOT listed in
# cups.pc; must be found separately.
# Debian/Ubuntu package: libcupsimage2-dev
find_library(CUPSIMAGE_LIBRARY
    NAMES   cupsimage
    HINTS   ${CUPS_LIBRARY_DIRS}
    DOC     "CUPS image conversion library (libcupsimage)"
)
if(NOT CUPSIMAGE_LIBRARY)
    message(FATAL_ERROR
        "\n[splix] libcupsimage not found."
        "\nInstall it with:"
        "\n  apt-get install libcupsimage2-dev\n"
    )
endif()

# libjbig — JBIG banded-compression used by algorithm 0x15 in rastertoqpdl.
# Modern Debian/Ubuntu ship it as -ljbig (package: libjbig-dev).
# The old module.mk called it -ljbig85 (Knoppix/ancient name) — that was
# patched to -ljbig as part of the 2025 build-system modernisation.
# Debian/Ubuntu package: libjbig-dev
find_library(JBIG_LIBRARY
    NAMES   jbig
    DOC     "JBIG compression library (libjbig)"
)
if(NOT JBIG_LIBRARY)
    message(FATAL_ERROR
        "\n[splix] libjbig not found."
        "\nInstall it with:"
        "\n  apt-get install libjbig-dev\n"
    )
endif()

# POSIX threads (pthreads or equivalent on the target platform)
find_package(Threads REQUIRED)

# ────────────────────────────────────────────────────────────────────────────
# CUPS Installation Path Variables
# ────────────────────────────────────────────────────────────────────────────
# Query the CUPS installation once at configure time so that:
#   a) The paths are baked into the compiled binaries (pstoqpdl uses execl()
#      with hardcoded strings).
#   b) The install() rules use the same paths as the CUPS subsystem expects.
#
# All six are exposed as CACHE STRING so a package maintainer can override
# them from the cmake command line without editing this file:
#   cmake -DSPLIX_FILTER_DIR=/usr/lib/cups/filter ..

find_program(CUPS_CONFIG_EXECUTABLE NAMES cups-config)
if(CUPS_CONFIG_EXECUTABLE)
    execute_process(
        COMMAND ${CUPS_CONFIG_EXECUTABLE} --serverbin
        OUTPUT_VARIABLE _CUPS_SERVERBIN
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${CUPS_CONFIG_EXECUTABLE} --datadir
        OUTPUT_VARIABLE _CUPS_DATADIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    execute_process(
        COMMAND pkg-config --variable=cups_serverbin cups
        OUTPUT_VARIABLE _CUPS_SERVERBIN
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND pkg-config --variable=cups_datadir cups
        OUTPUT_VARIABLE _CUPS_DATADIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if(NOT _CUPS_SERVERBIN)
    set(_CUPS_SERVERBIN "/usr/lib/cups")
endif()
if(NOT _CUPS_DATADIR)
    set(_CUPS_DATADIR "/usr/share/cups")
endif()

set(SPLIX_FILTER_DIR  "${_CUPS_SERVERBIN}/filter"
    CACHE STRING "Directory where CUPS looks for filter binaries")
set(SPLIX_PPD_DIR     "${_CUPS_DATADIR}/model"
    CACHE STRING "Directory where CUPS looks for PPD driver files")
set(SPLIX_DRV_DIR     "${_CUPS_DATADIR}/drv"
    CACHE STRING "Directory where CUPS looks for DRV source files")
set(SPLIX_PROFILE_DIR "${_CUPS_DATADIR}/profiles"
    CACHE STRING "Directory where CUPS looks for colour profiles")

# Runtime names of the raster converters called by pstoqpdl via fork/exec.
# pstoqpdl tries gstoraster first (Ghostscript-native), falls back to pstoraster.
set(PSTORASTER_BIN "pstoraster"
    CACHE STRING "Name of the PostScript-to-raster CUPS filter")
set(GSTORASTER_BIN "gstoraster"
    CACHE STRING "Name of the Ghostscript-to-raster CUPS filter")

message(STATUS "SpliX install paths (override with -DSPLIX_*):")
message(STATUS "  Filter dir  : ${SPLIX_FILTER_DIR}")
message(STATUS "  PPD dir     : ${SPLIX_PPD_DIR}")
message(STATUS "  DRV dir     : ${SPLIX_DRV_DIR}")
message(STATUS "  Profile dir : ${SPLIX_PROFILE_DIR}")

# ────────────────────────────────────────────────────────────────────────────
# Global Include Paths
# ────────────────────────────────────────────────────────────────────────────
include_directories(
    ${CUPS_INCLUDE_DIR}
    ${CUPS_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ────────────────────────────────────────────────────────────────────────────
# Core Static Library  (splix_core)
# ────────────────────────────────────────────────────────────────────────────
# Collect every .cpp in src/ that is NOT an entry-point binary; this becomes
# the shared core that both filter executables link against.
file(GLOB SPLIX_CORE_SRC "src/*.cpp")
list(REMOVE_ITEM SPLIX_CORE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rastertoqpdl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pstoqpdl.cpp"
)

add_library(splix_core STATIC ${SPLIX_CORE_SRC})
target_include_directories(splix_core PUBLIC
    ${CUPS_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Propagate CUPS compile flags (e.g. -D_CUPS_SOURCE) to consumers
target_compile_options(splix_core PUBLIC ${CUPS_CFLAGS_OTHER})

# ────────────────────────────────────────────────────────────────────────────
# Binary: rastertoqpdl
# ────────────────────────────────────────────────────────────────────────────
# Reads raster data from CUPS and writes a QPDL byte-stream to the printer.
# Links JBIG for the 0x13/0x15 compression algorithms.
add_executable(rastertoqpdl src/rastertoqpdl.cpp)
target_link_libraries(rastertoqpdl PRIVATE
    splix_core
    ${CUPS_LIBRARIES}       # -lcups
    ${CUPSIMAGE_LIBRARY}    # cupsImageOpen / cupsImageGetPixels
    ${JBIG_LIBRARY}         # jbg_enc_init / jbg_enc_out
    Threads::Threads
)
target_link_directories(rastertoqpdl PRIVATE ${CUPS_LIBRARY_DIRS})

# ────────────────────────────────────────────────────────────────────────────
# Binary: pstoqpdl
# ────────────────────────────────────────────────────────────────────────────
# PostScript pre-processor: inserts CMS colour correction data then chains to
# gstoraster/pstoraster → rastertoqpdl via fork/exec.
#
# CRITICAL: six paths must be baked in as compile-time string literals.
# Without these defines the file fails to compile:
#   pstoqpdl.cpp:69: error: 'RASTERDIR' undeclared
# They replace the src_pstoqpdl_cpp_FLAGS variable in the old module.mk.
add_executable(pstoqpdl src/pstoqpdl.cpp)
target_compile_definitions(pstoqpdl PRIVATE
    RASTERDIR="${SPLIX_FILTER_DIR}"
    RASTERTOQPDL="rastertoqpdl"
    GSTORASTER="${GSTORASTER_BIN}"
    PSTORASTER="${PSTORASTER_BIN}"
    CUPSPPD="${SPLIX_PPD_DIR}"
    CUPSPROFILE="${SPLIX_PROFILE_DIR}"
)
target_link_libraries(pstoqpdl PRIVATE
    splix_core
    ${CUPS_LIBRARIES}
    ${CUPSIMAGE_LIBRARY}
    Threads::Threads
)
target_link_directories(pstoqpdl PRIVATE ${CUPS_LIBRARY_DIRS})

# ────────────────────────────────────────────────────────────────────────────
# Install: Filter Binaries
# ────────────────────────────────────────────────────────────────────────────
install(TARGETS rastertoqpdl pstoqpdl
    RUNTIME DESTINATION "${SPLIX_FILTER_DIR}"
)

# ────────────────────────────────────────────────────────────────────────────
# Install: PPD Files
# ────────────────────────────────────────────────────────────────────────────
# The PPD files for every supported printer model are pre-compiled and
# committed to the ppd/ directory.  We install them directly — no cups-ppdc
# runtime tool required.  This mirrors what the old ppd/Makefile did: it ran
# cups-ppdc at development time, committed the output, then the install target
# simply copied the files.
#
# Brand → CUPS sub-directory mapping (from ppd/Makefile):
#   SAMSUNG : clp clx m[0-9] ml scx sf  → samsung/
#   XEROX   : ph wc wcpe               → xerox/
#   DELL    : 1100 1110                → dell/
#   LEXMARK : x215                     → lexmark/
#   TOSHIBA : es                       → toshiba/
#
# HP (laser10x, laser13x) has no pre-built .ppd files in the repo; they are
# generated on-demand via cups-ppdc from hp.drv.in and not installed here.
#
# Language variants (French: *fr.ppd, Brazilian Portuguese: *pt.ppd) are
# included automatically by the *.ppd glob.

# Samsung ----------------------------------------------------------------
install(
    DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/ppd/"
    DESTINATION "${SPLIX_PPD_DIR}/samsung"
    FILES_MATCHING
        PATTERN "clp*.ppd"         # Colour laser
        PATTERN "clx*.ppd"         # Colour multifunction
        PATTERN "m[0-9]*.ppd"      # M-series (M2020, M2070, M262x …)
        PATTERN "ml*.ppd"          # Monochrome laser
        PATTERN "scx*.ppd"         # Monochrome multifunction
        PATTERN "sf*.ppd"          # Fax-capable multifunction
)

# Xerox ------------------------------------------------------------------
# Note: wcpe* is a subset of wc*, so a single wc*.ppd pattern covers both.
install(
    DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/ppd/"
    DESTINATION "${SPLIX_PPD_DIR}/xerox"
    FILES_MATCHING
        PATTERN "ph*.ppd"          # Phaser series
        PATTERN "wc*.ppd"          # WorkCentre series (wc* and wcpe*)
)

# Dell -------------------------------------------------------------------
install(
    DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/ppd/"
    DESTINATION "${SPLIX_PPD_DIR}/dell"
    FILES_MATCHING
        PATTERN "1100*.ppd"
        PATTERN "1110*.ppd"
)

# Lexmark ----------------------------------------------------------------
install(
    DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/ppd/"
    DESTINATION "${SPLIX_PPD_DIR}/lexmark"
    FILES_MATCHING
        PATTERN "x215*.ppd"
)

# Toshiba ----------------------------------------------------------------
install(
    DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/ppd/"
    DESTINATION "${SPLIX_PPD_DIR}/toshiba"
    FILES_MATCHING
        PATTERN "es*.ppd"
)
