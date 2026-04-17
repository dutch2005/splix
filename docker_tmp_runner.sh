#!/usr/bin/env bash
set -euo pipefail

echo " [1/4] Installing dependencies..."
apt-get update -y > /dev/null
apt-get install -y --fix-missing \
    build-essential cmake \
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
    TOOLCHAIN_FLAG="-DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/toolchain-arm64.cmake"
else
    TOOLCHAIN_FLAG=""
fi

echo " [2/4] Configuring CMake..."
if [ "${DEB_ARCH:-amd64}" = "arm64" ]; then
    cmake --preset linux-arm64-release -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
else
    cmake --preset linux-amd64-release -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
fi

echo " [3/4] Compiling..."
if [ "${DEB_ARCH:-amd64}" = "arm64" ]; then
    cmake --build --preset linux-arm64-release --parallel "$(nproc)"
    cd build-arm64
else
    cmake --build --preset linux-amd64-release --parallel "$(nproc)"
    cd build-amd64
fi

echo " [4/4] Generating Debian Package (.deb)..."
cpack
cp *.deb /workspace/artifacts/

echo "=================================================="
echo " ??? SUCCESS! Package generated securely."
echo "=================================================="
ls -l /workspace/artifacts/
