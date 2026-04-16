cmake_minimum_required(VERSION 3.10)

# cmake_policy CMP0048: project() manages VERSION variables.
# Required by cmake_minimum_required >= 3.0 but explicit is clearer.
cmake_policy(SET CMP0048 NEW)

project(splix
    VERSION     2.0.2
    DESCRIPTION "Samsung / Xerox / Dell printer CUPS driver (QPDL)"
    LANGUAGES   CXX
)

# ────────────────────────────────────────────────────────────────────────────
# C++ Standard
# ────────────────────────────────────────────────────────────────────────────
# C++11 is the minimum needed for <new>, range-for, and modern nullptr usage
# that is already present in the 2008-era source.  Keeping it at 11 avoids
# dragging in C++14/17 features that would raise the bar for packagers.
set(CMAKE_CXX_STANDARD          11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS        OFF)   # -std=c++11, not -std=gnu++11

# ────────────────────────────────────────────────────────────────────────────
# Dependency Detection
# ────────────────────────────────────────────────────────────────────────────

# CUPS core — header path + -lcups
find_package(CUPS REQUIRED)

# libcupsimage — CMake's built-in FindCUPS only exposes -lcups; cupsimage
# must be discovered separately.  Both rastertoqpdl and pstoqpdl need it.
find_library(CUPSIMAGE_LIBRARY
    NAMES cupsimage
    HINTS ${CUPS_LIBRARY_DIRS}
    DOC   "CUPS image conversion library"
    REQUIRED
)

# libjbig — JBIG banded-compression; used by the 0x15 algorithm in
# rastertoqpdl.  The old Makefile called it -ljbig85 on ancient systems;
# modern Debian/Ubuntu ship it as -ljbig (libjbig-dev).
find_library(JBIG_LIBRARY
    NAMES jbig
    DOC   "JBIG compression library"
    REQUIRED
)

# POSIX threads
find_package(Threads REQUIRED)

# pkg-config — used at *configure* time to query CUPS installation paths
# that are not exposed by CMake's FindCUPS module.
find_package(PkgConfig REQUIRED)
pkg_check_modules(CUPS_PC REQUIRED cups)

# ────────────────────────────────────────────────────────────────────────────
# CUPS Installation Path Variables
# ────────────────────────────────────────────────────────────────────────────
# These are resolved once at cmake-configure time via pkg-config and then
# baked into both the compiled binaries (via preprocessor defines) and the
# CMake install() rules.
#
# Every variable is a CACHE STRING so a package maintainer can override it on
# the cmake command line without editing this file:
#
#   cmake -DSPLIX_FILTER_DIR=/usr/lib/cups/filter ..
#
# The defaults are queried from the CUPS installation on the build host,
# which guarantees they are correct for the target system inside the Docker
# container used by CI.

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

set(SPLIX_FILTER_DIR  "${_CUPS_SERVERBIN}/filter"
    CACHE STRING "Directory where CUPS looks for filter binaries")
set(SPLIX_PPD_DIR     "${_CUPS_DATADIR}/model"
    CACHE STRING "Directory where CUPS looks for PPD files")
set(SPLIX_DRV_DIR     "${_CUPS_DATADIR}/drv"
    CACHE STRING "Directory where CUPS looks for DRV files")
set(SPLIX_PROFILE_DIR "${_CUPS_DATADIR}/profiles"
    CACHE STRING "Directory where CUPS looks for colour profiles")

# The names of the raster-conversion helpers called by pstoqpdl at runtime.
# pstoraster is the POSIX name; gstoraster is the Ghostscript-native name.
# The binary tries gstoraster first, then falls back to pstoraster.
set(PSTORASTER_BIN "pstoraster"
    CACHE STRING "Name of the PostScript-to-raster CUPS filter")
set(GSTORASTER_BIN "gstoraster"
    CACHE STRING "Name of the Ghostscript-to-raster CUPS filter")

message(STATUS "SpliX install paths:")
message(STATUS "  Filter dir : ${SPLIX_FILTER_DIR}")
message(STATUS "  PPD dir    : ${SPLIX_PPD_DIR}")
message(STATUS "  DRV dir    : ${SPLIX_DRV_DIR}")
message(STATUS "  Profile dir: ${SPLIX_PROFILE_DIR}")

# ────────────────────────────────────────────────────────────────────────────
# Global Include Paths
# ────────────────────────────────────────────────────────────────────────────
include_directories(
    ${CUPS_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ────────────────────────────────────────────────────────────────────────────
# Core Static Library  (splix_core)
# ────────────────────────────────────────────────────────────────────────────
# Gather every .cpp in src/ that is NOT a main() entry point.
# Using GLOB here is intentional: the source list is frozen (2008) and no
# new files are expected.  If a file is added the developer should re-run
# cmake to regenerate.
file(GLOB SPLIX_CORE_SRC "src/*.cpp")
list(REMOVE_ITEM SPLIX_CORE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rastertoqpdl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pstoqpdl.cpp"
)

add_library(splix_core STATIC ${SPLIX_CORE_SRC})

# Propagate include directories to any target that links splix_core
target_include_directories(splix_core PUBLIC
    ${CUPS_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ────────────────────────────────────────────────────────────────────────────
# Filter Binary: rastertoqpdl
# ────────────────────────────────────────────────────────────────────────────
# Reads raster data from CUPS and emits a QPDL byte-stream to the printer.
# Requires JBIG for the 0x13/0x15 compression algorithms.
add_executable(rastertoqpdl src/rastertoqpdl.cpp)
target_link_libraries(rastertoqpdl PRIVATE
    splix_core
    ${CUPS_LIBRARIES}
    ${CUPSIMAGE_LIBRARY}    # cupsImageOpen / cupsImageGetPixels
    ${JBIG_LIBRARY}         # jbg_enc_init / jbg_enc_out (algo 0x15)
    Threads::Threads
)

# ────────────────────────────────────────────────────────────────────────────
# Filter Binary: pstoqpdl
# ────────────────────────────────────────────────────────────────────────────
# PostScript pre-processor: inserts CMS colour correction data then hands off
# to gstoraster/pstoraster → rastertoqpdl via fork/exec.
#
# This binary uses six paths that MUST be baked in at compile time as
# preprocessor string literals.  The old Makefile supplied them via
# src_pstoqpdl_cpp_FLAGS.  Without these defines the file does not compile:
#   pstoqpdl.cpp:69: error: 'RASTERDIR' undeclared
add_executable(pstoqpdl src/pstoqpdl.cpp)
target_compile_definitions(pstoqpdl PRIVATE
    # Where CUPS filter binaries live (e.g. /usr/lib/cups/filter)
    RASTERDIR="${SPLIX_FILTER_DIR}"
    # The name of this project's own raster filter
    RASTERTOQPDL="rastertoqpdl"
    # Fallback raster converters tried at runtime (in order: gs first)
    GSTORASTER="${GSTORASTER_BIN}"
    PSTORASTER="${PSTORASTER_BIN}"
    # Where PPD model files are installed
    CUPSPPD="${SPLIX_PPD_DIR}"
    # Where CUPS colour profiles are installed
    CUPSPROFILE="${SPLIX_PROFILE_DIR}"
)
target_link_libraries(pstoqpdl PRIVATE
    splix_core
    ${CUPS_LIBRARIES}
    ${CUPSIMAGE_LIBRARY}
    Threads::Threads
)

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
# committed to the ppd/ directory.  We install them directly with no
# cups-ppdc runtime dependency, which is exactly what the old ppd/Makefile
# did (it ran cups-ppdc at *development* time, committed the output, and then
# the install target just copied the resulting .ppd files).
#
# Brand → CUPS sub-directory mapping is taken verbatim from ppd/Makefile:
#   DELL    := 1100 1110
#   SAMSUNG := clp* clx* m* ml* scx* sf*
#   XEROX   := ph* wc* wcpe*
#   LEXMARK := x215*
#   TOSHIBA := es*
#
# The French (fr) and Brazilian Portuguese (pt_BR → pt suffix) variants are
# included by the *.ppd glob so they install automatically.
#
# HP (laser10x, laser13x) has no pre-built .ppd files in the tree; those are
# generated on-demand via cups-ppdc from hp.drv.in and are not installed here.

# Samsung ----------------------------------------------------------------
install(
    DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/ppd/"
    DESTINATION "${SPLIX_PPD_DIR}/samsung"
    FILES_MATCHING
        # Colour laser / multifunction
        PATTERN "clp*.ppd"
        PATTERN "clx*.ppd"
        # M-series (M2020, M2070, M262x …)
        PATTERN "m[0-9]*.ppd"
        # Monochrome laser
        PATTERN "ml*.ppd"
        # Monochrome multifunction
        PATTERN "scx*.ppd"
        # Fax-capable multifunction
        PATTERN "sf*.ppd"
)

# Xerox ------------------------------------------------------------------
install(
    DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/ppd/"
    DESTINATION "${SPLIX_PPD_DIR}/xerox"
    FILES_MATCHING
        # Phaser series
        PATTERN "ph*.ppd"
        # WorkCentre series (wc3025, wc3119, wc3215, wc3225, wcpe16, wcpe114e …)
        PATTERN "wc*.ppd"
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
