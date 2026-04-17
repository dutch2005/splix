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
    CMAKE_PRESET="ci-arm64"
else
    TARGET_ARCH="amd64"
    CMAKE_PRESET="ci-amd64"
fi

echo ">> Spawning debian:oldstable container... (This may take a minute)"

docker run --rm \
  --platform linux/amd64 \
  -v "${ROOT_DIR}:/workspace" \
  -v "${ROOT_DIR}/.ccache:/root/.ccache" \
  -w /workspace \
  -e CCACHE_DIR=/root/.ccache \
  -e DEB_ARCH="${TARGET_ARCH}" \
  -e CMAKE_PRESET="${CMAKE_PRESET}" \
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
    cmake --preset "${CMAKE_PRESET}"

    echo " [3/4] Compiling..."
    cmake --build --preset "${CMAKE_PRESET}"

    echo " [4/4] Generating Debian Package (.deb) via CPack..."
    cd /tmp/splix-build
    cpack -G DEB

    cp -v /tmp/splix-build/*.deb /workspace/artifacts/

    echo "=================================================="
    echo " ✅ SUCCESS! Package generated."
    echo "=================================================="
    ls -lh /workspace/artifacts/
  '

echo "Check your local /artifacts directory for the generated .deb file!"
