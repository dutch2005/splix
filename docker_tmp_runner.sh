#!/usr/bin/env bash
set -euo pipefail

echo " [1/4] Installing dependencies..."
apt-get update -y > /dev/null
apt-get install -y --fix-missing \
    build-essential cmake git xxd \
    cups libcups2-dev libcupsimage2-dev libjbig-dev \
    pkg-config ccache > /dev/null

export PATH="/usr/lib/ccache:${PATH}"

if [ "${DEB_ARCH:-amd64}" = "arm64" ]; then
    echo "       Installing ARM64 cross-compile toolchain..."
    dpkg --add-architecture arm64
    apt-get update -y > /dev/null
    apt-get install -y --fix-missing \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    libcups2-dev:arm64 \
    libcupsimage2-dev:arm64 \
    libjbig-dev:arm64 > /dev/null
    CMAKE_PRESET="ci-arm64"
else
    CMAKE_PRESET="ci-amd64"
fi

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
ls -l /workspace/artifacts/
