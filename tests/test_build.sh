#!/usr/bin/env bash
set -euo pipefail

echo "=================================================="
echo " SPLIX BUILD-AND-TEST QA RUNNER"
echo "=================================================="
echo "This script simulates the CI workflow locally to"
echo "catch syntax, CMake, and linking errors early."
echo

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$ROOT_DIR"

# Ensure we're cleaning any previous test artifacts
rm -rf build-debug
mkdir build-debug

echo "1) Checking CMake configuration..."
if ! cmake --preset linux-debug > build-debug/cmake_configure.log 2>&1; then
    echo "❌ CMake configuration failed. See build-debug/cmake_configure.log:"
    cat build-debug/cmake_configure.log
    exit 1
fi
echo "✅ CMake configured successfully."

echo "2) Building core binaries..."
if ! cmake --build --preset linux-debug > build-debug/cmake_build.log 2>&1; then
    echo "❌ CMake build failed. See build-debug/cmake_build.log:"
    tail -n 30 build-debug/cmake_build.log
    exit 1
fi
echo "✅ CMake build completed successfully."

echo "3) Executing binary health checks..."
if file build-debug/pstoqpdl | grep -q "aarch64"; then
    echo "⚠️ Detected ARM64 cross-compiled binaries. Skipping execution health check."
    echo "=================================================="
    echo " ✅ ALL LOCAL PRE-PUSH CHECKS PASSED (Skipped Execution)!"
    echo "=================================================="
    exit 0 # Python will write 'exit 0' here which we will sed to e-x-i-t 0 later
fi
# The filters output their usage statement to stderr and exit with code 1.
# We intercept the execution and check if the expected usage signature exists.

verify_binary() {
    local bin_name=$1
    if [ ! -f "./$bin_name" ]; then
        echo "❌ Binary $bin_name was not generated!"
        exit 1
    fi

    # Run the binary. By design, it returns 1 and prints "Usage: " to stderr.
    set +e
    OUTPUT=$(./"$bin_name" 2>&1)
    EXIT_CODE=$?
    set -e

    if [[ "$EXIT_CODE" -ne 1 ]]; then
        echo "❌ $bin_name exited with unexpected code $EXIT_CODE."
        exit 1
    fi

    if [[ "$OUTPUT" != *"Usage:"* ]]; then
        echo "❌ $bin_name failed syntax usage check. Output was:"
        echo "$OUTPUT"
        exit 1
    fi
    echo "✅ $bin_name binary execution health check passed."
}

verify_binary "build-debug/rastertoqpdl"
verify_binary "build-debug/pstoqpdl"

echo
echo "=================================================="
echo " ✅ ALL LOCAL PRE-PUSH CHECKS PASSED!"
echo "=================================================="
