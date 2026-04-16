#!/usr/bin/env bash
set -euo pipefail

TARGET_ARCH=${1:-amd64}

echo "=================================================="
echo " SPLIX CI/CD LOCAL DOCKER SIMULATOR"
echo " Architecture: $TARGET_ARCH"
echo "=================================================="
echo "This script runs the exact GitHub Actions logic"
echo "inside a local Docker container."
echo

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$ROOT_DIR"
mkdir -p artifacts
mkdir -p .ccache

if [ "$TARGET_ARCH" = "arm64" ]; then
    TOOLCHAIN_FLAG="-DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/toolchain-arm64.cmake"
else
    TARGET_ARCH="amd64"
    TOOLCHAIN_FLAG=""
fi

echo ">> Spawning debian:oldstable container... (This may take a minute)"

docker run --rm \
  --platform linux/amd64 \
  -v "${ROOT_DIR}:/workspace" \
  -v "${ROOT_DIR}/.ccache:/root/.ccache" \
  -w /workspace \
  -e CCACHE_DIR=/root/.ccache \
  -e DEB_ARCH="${TARGET_ARCH}" \
  -e TOOLCHAIN_FLAG="${TOOLCHAIN_FLAG}" \
  -e GITHUB_REF_NAME="2.0.2" \
  debian:oldstable \
  bash -c '
    set -euo pipefail

    echo " [1/4] Installing dependencies..."
    apt-get update -y > /dev/null
    apt-get install -y --fix-missing \
      build-essential cmake \
      cups libcups2-dev libcupsimage2-dev libjbig-dev \
      pkg-config ccache > /dev/null

    if [ "${DEB_ARCH}" = "arm64" ]; then
      echo "       Installing ARM64 cross-compile toolchain..."
      dpkg --add-architecture arm64
      apt-get update -y > /dev/null
      apt-get install -y --fix-missing \
        gcc-aarch64-linux-gnu \
        g++-aarch64-linux-gnu \
        libcups2-dev:arm64 \
        libcupsimage2-dev:arm64 \
        libjbig-dev:arm64 > /dev/null
    fi

    export PATH="/usr/lib/ccache:${PATH}"

    echo " [2/4] Configuring CMake..."
    rm -rf /tmp/splix-build
    mkdir  /tmp/splix-build

    cmake \
      -S /workspace \
      -B /tmp/splix-build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
      ${TOOLCHAIN_FLAG} > /dev/null

    echo " [3/4] Compiling..."
    cmake --build /tmp/splix-build --parallel "$(nproc)" > /dev/null

    echo " [4/4] Generating Debian Package (.deb)..."
    PKG_VERSION="${GITHUB_REF_NAME:-2.0.2}"
    cd /tmp/splix-build
    
    PKG_DIR="/tmp/splix-pkg"
    mkdir -p "${PKG_DIR}/DEBIAN"
    
    echo "Package: splix" > "${PKG_DIR}/DEBIAN/control"
    echo "Version: ${PKG_VERSION}" >> "${PKG_DIR}/DEBIAN/control"
    echo "Architecture: ${DEB_ARCH}" >> "${PKG_DIR}/DEBIAN/control"
    echo "Maintainer: github-actions@noreply.github.com" >> "${PKG_DIR}/DEBIAN/control"
    echo "Depends: cups, libcups2, libcupsimage2, libjbig0" >> "${PKG_DIR}/DEBIAN/control"
    echo "Description: SpliX printer drivers for Samsung and Xerox printers" >> "${PKG_DIR}/DEBIAN/control"

    make DESTDIR="${PKG_DIR}" install > /dev/null
    dpkg-deb --build "${PKG_DIR}" "/workspace/artifacts/splix-ubuntu-${PKG_VERSION}-${DEB_ARCH}.deb" > /dev/null

    echo "=================================================="
    echo " ✅ SUCCESS! Package generated securely."
    echo "=================================================="
  '

echo "Check your local /artifacts directory for the generated .deb file!"
