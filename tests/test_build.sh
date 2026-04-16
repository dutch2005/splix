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
rm -rf build-test
mkdir build-test
cd build-test

echo "1) Checking CMake configuration..."
if ! cmake .. > cmake_configure.log 2>&1; then
    echo "❌ CMake configuration failed. See cmake_configure.log:"
    cat cmake_configure.log
    exit 1
fi
echo "✅ CMake configured successfully."

echo "2) Building core binaries..."
if ! cmake --build . > cmake_build.log 2>&1; then
    echo "❌ CMake build failed. See cmake_build.log:"
    tail -n 30 cmake_build.log
    exit 1
fi
echo "✅ CMake build completed successfully."

echo "3) Executing binary health checks..."
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

verify_binary "rastertoqpdl"
verify_binary "pstoqpdl"

echo
echo "=================================================="
echo " ✅ ALL LOCAL PRE-PUSH CHECKS PASSED!"
echo "=================================================="
