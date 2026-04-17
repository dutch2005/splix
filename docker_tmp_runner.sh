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
echo "Architecture: ${DEB_ARCH:-amd64}" >> "${PKG_DIR}/DEBIAN/control"
echo "Maintainer: github-actions@noreply.github.com" >> "${PKG_DIR}/DEBIAN/control"
echo "Depends: cups, libcups2, libcupsimage2, libjbig0" >> "${PKG_DIR}/DEBIAN/control"
echo "Description: SpliX printer drivers for Samsung and Xerox printers" >> "${PKG_DIR}/DEBIAN/control"

make DESTDIR="${PKG_DIR}" install > /dev/null
dpkg-deb --build "${PKG_DIR}" "/workspace/artifacts/splix-ubuntu-${PKG_VERSION}-${DEB_ARCH:-amd64}.deb"

echo "=================================================="
echo " ??? SUCCESS! Package generated securely."
echo "=================================================="
ls -l /workspace/artifacts/
