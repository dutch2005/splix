# Building SpliX

This document explains the build system, what changed from the original, and
how to build SpliX locally or understand what the CI pipeline does.

---

## Table of Contents

1. [Quick Start (Local Build)](#quick-start)
2. [What Changed and Why](#what-changed-and-why)
3. [File-by-File Change Log](#file-by-file-change-log)
4. [CMake Variable Reference](#cmake-variable-reference)
5. [CI / GitHub Actions](#ci--github-actions)
6. [PPD File Map (Brand → Directory)](#ppd-file-map)
7. [Troubleshooting](#troubleshooting)

---

## Quick Start

### Prerequisites

```bash
# Debian / Ubuntu
sudo apt-get install \
    build-essential cmake \
    libcups2-dev libcupsimage2-dev \
    libjbig-dev pkg-config
```

### Build

```bash
# Out-of-tree build (recommended — keeps the source tree clean)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Install

```bash
# System-wide (requires root)
sudo cmake --install build

# Staging directory (for packaging)
cmake --install build --destdir /tmp/splix-stage
```

### Override install paths

```bash
cmake -S . -B build \
    -DSPLIX_FILTER_DIR=/usr/lib/cups/filter \
    -DSPLIX_PPD_DIR=/usr/share/cups/model
```

---

## What Changed and Why

### Old build system: the legacy `Makefile`

SpliX was written in 2006–2008. The original build system is a hand-rolled
recursive Make framework spread across:

| File | Purpose |
|---|---|
| `Makefile` | Root orchestrator — 390 lines of macro-heavy GNU Make |
| `module.mk` | Project-level flags, targets, library names |
| `src/module.mk` | Source file lists for each binary |
| `rules.mk` | Link rules, install rules, `drv`/`ppd` targets |
| `ppd/Makefile` | PPD file compilation and installation (all 5 brands) |

**Problems with the old system (as of 2025):**

1. **Hard-coded library name `-ljbig85`** — the Knoppix-era name for the JBIG
   library. Modern Debian and Ubuntu ship it as `-ljbig`. Every CI attempt
   had to patch this at runtime with `sed`.

2. **Hard-coded macOS paths** — `-I/opt/local/include` and `-L/opt/local/lib`
   (Homebrew/MacPorts paths) were unconditionally added to Linux builds,
   cluttering the compiler command and confusing cl-cache tools.

3. **`pkg-config` invoked inside recipe strings** — paths like `CUPSFILTER`
   were resolved by running backtick sub-shells inside Makefile variable
   assignments, making cross-compilation and reproducible builds fragile.

4. **No out-of-tree build support** — running `make` always wrote object files
   into the source tree, making it impossible to build multiple configurations
   simultaneously.

5. **Packaging (`checkinstall`) broke consistently** — the `cp *.deb` glob
   at the end of the workflow expanded in the wrong shell context, producing
   silent failures with no artifact uploaded.

### New build system: `CMakeLists.txt`

CMake replaces all five files above with a single, self-contained
`CMakeLists.txt`. It uses standard CMake idioms that package maintainers and
IDE tooling understand natively.

**Key improvements:**

| Concern | Old Makefile | New CMake |
|---|---|---|
| Library name | `-ljbig85` (hardcoded, wrong) | `find_library(JBIG_LIBRARY NAMES jbig)` |
| `libcupsimage` | Implicit via `pkg-config --libs cups` | Explicit `find_library(CUPSIMAGE_LIBRARY ...)` |
| CUPS paths | `pkg-config` backtick inside recipe | `execute_process()` at configure time → `CACHE STRING` |
| pstoqpdl defines | Six `-D` flags in `module.mk` | `target_compile_definitions(pstoqpdl PRIVATE ...)` |
| Out-of-tree build | Not supported | Native (`cmake -B build`) |
| Override paths | Edit `module.mk` | `-DSPLIX_FILTER_DIR=...` on cmake command line |
| Cross-arch | Fragile env-var hacks | CMake's toolchain abstraction |

---

## File-by-File Change Log

### `module.mk` — 3 lines changed

| Line | Before | After | Reason |
|---|---|---|---|
| 31 | `-I/opt/local/include` | `-std=c++11` | Remove dead macOS Homebrew path; pin C++ standard |
| 34 | `$(LDFLAGS) -L/opt/local/lib` | `$(LDFLAGS)` | Remove dead macOS Homebrew lib path |
| 51 | `-ljbig85` | `-ljbig` | Fix the library name to match modern Debian/Ubuntu packaging |

> **Note:** `module.mk` is kept for users who still want to build with `make`.
> The CMake build does not use it — it detects everything from scratch.

---

### `CMakeLists.txt` — new file

Complete replacement for the Makefile build system. See the inline comments
in the file for rationale on every decision. Key sections:

| CMake section | Replaces |
|---|---|
| `find_package(CUPS)` + `find_library(CUPSIMAGE_LIBRARY)` | `pkg-config --libs cups` + `-lcupsimage` in `module.mk` |
| `find_library(JBIG_LIBRARY)` | `-ljbig85` (patched to `-ljbig`) in `module.mk` |
| `execute_process(pkg-config --variable=...)` | Backtick expansions in `module.mk` for paths |
| `add_library(splix_core STATIC ...)` | Implicit object list from `src/module.mk` |
| `target_compile_definitions(pstoqpdl PRIVATE ...)` | `src_pstoqpdl_cpp_FLAGS` in `module.mk` |
| `install(TARGETS ...)` | `install:` target in `rules.mk` |
| `install(DIRECTORY ppd/ ...)` | `install:` target in `ppd/Makefile` (all 5 brands) |

---

### `.github/workflows/Build.yml` — rewritten

The repo previously contained **three** competing workflow files, all with the
same structural bugs:

| Old file | Status |
|---|---|
| `Build.yml` | Retained and rewritten |
| `Build Splix Drivers V3 (Clean Detection).yml` | **Deleted** — duplicate, same bugs |
| `build-works-but-not-complete.yml` | **Deleted** — incomplete, same bugs |

**Bugs fixed in the new workflow:**

| Bug | Impact | Fix |
|---|---|---|
| `cp *.deb` glob expansion | `.deb` never copied to artifacts → no artifact uploaded | Replaced with `find -name "*.deb" -exec cp` |
| `checkinstall` + wrong working directory | `checkinstall` wrote `.deb` into source dir, `cp` looked in build dir | Build in `/tmp/splix-build`; `cd` there before `checkinstall`; `find` in same dir |
| `rastertoqpdl_LIBS` env var + `sed` patch both active | Double-patching caused linker failures on some runs | Removed env var overrides; library name fixed in source (`module.mk`) |
| `pkg_config_arch` defined but unused | Silent mis-configuration | Variable removed; CMake handles this automatically |
| `fail-fast: true` (default) | ARM64 failure killed AMD64 build and vice versa | `fail-fast: false` added to matrix |
| No artifact failure detection | Workflow showed green even with no `.deb` | `if-no-files-found: error` on upload step |
| No release support | Manual process required to attach `.deb` to a tag | `softprops/action-gh-release` step on `refs/tags/v*` |
| `ccache` not wired to CMake | CMake bypassed `PATH`-based wrappers | `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` flag |

---

## CMake Variable Reference

All variables can be overridden on the `cmake` command line with `-D`.

| Variable | Default (from pkg-config) | Description |
|---|---|---|
| `SPLIX_FILTER_DIR` | `$(cups_serverbin)/filter` | Where CUPS filter binaries are installed |
| `SPLIX_PPD_DIR` | `$(cups_datadir)/model` | Where PPD driver files are installed |
| `SPLIX_DRV_DIR` | `$(cups_datadir)/drv` | Where DRV source files are installed |
| `SPLIX_PROFILE_DIR` | `$(cups_datadir)/profiles` | Where colour profiles are installed |
| `PSTORASTER_BIN` | `pstoraster` | Runtime name of the PS-to-raster filter |
| `GSTORASTER_BIN` | `gstoraster` | Runtime name of the GS-to-raster filter |

---

## CI / GitHub Actions

The single workflow file (`.github/workflows/Build.yml`) builds inside a
`debian:oldstable` Docker container for maximum binary compatibility with
older Debian and Ubuntu systems.

### Matrix

| Arch | Runner | Method |
|---|---|---|
| `amd64` | `ubuntu-latest` (x86-64) | Native container |
| `arm64` | `ubuntu-latest` + QEMU | Emulated via `docker/setup-qemu-action` |

### Build steps (inside Docker)

```
apt-get install cmake libcups2-dev libcupsimage2-dev libjbig-dev ...
cmake -S /workspace -B /tmp/splix-build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build /tmp/splix-build --parallel $(nproc)
checkinstall ... cmake --install . --destdir /tmp/splix-install
find /tmp/splix-build -name "*.deb" -exec cp {} /workspace/artifacts/ \;
```

### Triggers

| Event | Action |
|---|---|
| Push to `main` / `master` | Builds both arches, uploads artifacts |
| Pull request to `main` | Builds both arches (validation only) |
| Push a `v*` tag | Builds both arches + attaches `.deb` to GitHub Release |
| `workflow_dispatch` | Manual trigger from the Actions UI |

---

## PPD File Map

The following table mirrors the `ppd/Makefile` brand-to-directory mapping
and is what is physically implemented in the `install(DIRECTORY ...)` rules
in `CMakeLists.txt`.

| CMake pattern | CUPS install directory | Printer brand |
|---|---|---|
| `clp*.ppd`, `clx*.ppd`, `m[0-9]*.ppd`, `ml*.ppd`, `scx*.ppd`, `sf*.ppd` | `$(CUPSPPD)/samsung/` | Samsung |
| `ph*.ppd`, `wc*.ppd` | `$(CUPSPPD)/xerox/` | Xerox |
| `1100*.ppd`, `1110*.ppd` | `$(CUPSPPD)/dell/` | Dell |
| `x215*.ppd` | `$(CUPSPPD)/lexmark/` | Lexmark |
| `es*.ppd` | `$(CUPSPPD)/toshiba/` | Toshiba |

> **HP (laser10x, laser13x):** The original `ppd/Makefile` lists HP models
> but no pre-built `.ppd` files exist in the repository for them.  They were
> generated from `hp.drv.in` at development time.  If HP support is needed,
> run `cups-ppdc ppd/hp.drv.in -d ppd/` and commit the resulting files.

---

## Troubleshooting

### `Could NOT find CUPS`

Install the development headers: `apt-get install libcups2-dev`

### `Could not find CUPSIMAGE_LIBRARY`

Install: `apt-get install libcupsimage2-dev`

### `Could not find JBIG_LIBRARY`

Install: `apt-get install libjbig-dev`

### `pstoqpdl.cpp: error: 'RASTERDIR' undeclared`

You are building with the old `make` (not CMake).  
With CMake, these are supplied via `target_compile_definitions`.  
With the old Makefile, verify `module.mk` still has `src_pstoqpdl_cpp_FLAGS`.

### `checkinstall: no .deb produced`

Check that you `cd` into the CMake build directory **before** running
`checkinstall`.  The `.deb` is written to the current working directory.

### ARM64 build is very slow

Expected — it runs under QEMU.  The `--parallel $(nproc)` flag and the
`ccache` cache warm-up on the second run mitigate this significantly.
Expect 15–25 minutes for a cold ARM64 build.
