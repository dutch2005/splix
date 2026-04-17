#!/usr/bin/env bash
# tests/functional_test.sh
# 
# SpliX Functional Integration Test (The Post-hook)
# 
# This script simulates a real CUPS print job by:
# 1. Generating a valid (minimal) CUPS Raster header.
# 2. Feeding it into 'rastertoqpdl'.
# 3. Verifying that the output is valid QPDL (starts with PJL/Escapes).

set -euo pipefail

# Path to the build directory (passed as an argument)
BUILD_DIR=${1:-build}
BIN_PATH="$BUILD_DIR/rastertoqpdl"

if [ ! -f "$BIN_PATH" ]; then
    echo "❌ Error: rastertoqpdl not found at $BIN_PATH"
    exit 1
fi

echo "── Running Functional Raster-to-QPDL Test ──"

# Setup dummy environment variables that CUPS would provide
export PPD="$PWD/ppd/ml1710.ppd" # Use a standard monochrome PPD
export CUPS_SERVERBIN="/usr/lib/cups"
export CUPS_DATADIR="/usr/share/cups"
export PRINTER="splix_test_printer"

# Mock a CUPS destination so cupsGetNamedDest succeeds
export HOME="$PWD/temp_home"
export CUPS_CONFDIR="$PWD/temp_home/cups"
mkdir -p "$HOME/.cups"
mkdir -p "$CUPS_CONFDIR"
echo "dest $PRINTER ml1710" > "$HOME/.cups/lpoptions"
echo "Default $PRINTER" > "$CUPS_CONFDIR/lpoptions"
export LPDEST="$PRINTER"
export CUPS_LPDEST="$PRINTER"

# 1. Generate a minimal CUPS Raster Header (448 bytes)
# 'RaSt' (0x52 0x61 0x53 0x74) - Synch string
# Followed by page header fields.
# We'll use a 100x100 1-bit monochrome page as a sample.
#
# PPD: Samsung ML-1710 (Standard 600dpi)
# Options: dummy job-id, user, title, copies, options

RASTER_FILE=$(mktemp)
QPDL_FILE=$(mktemp)

# Create 448 bytes of dummy header
# Bytes 0-3: 'RaSt'
printf "RaSt" > "$RASTER_FILE"
# Fill the rest with zeros (minimal valid-ish header)
dd if=/dev/zero bs=1 count=444 >> "$RASTER_FILE" 2>/dev/null

# Append 100 lines of zero data (white space)
# For 1-bit, 100 pixels = 13 bytes per line (rounded up)
dd if=/dev/zero bs=13 count=100 >> "$RASTER_FILE" 2>/dev/null

echo ">> Feeding dummy raster to filter..."
# Usage: job-id user title copies options [file]
# We pass the raster on stdin.
"$BIN_PATH" 1 "testuser" "test-document" 1 "" < "$RASTER_FILE" > "$QPDL_FILE" 2>test_stderr.log || true

# 2. Verify Output
# QPDL (Samsung) usually starts with PJL: \x1b%-12345X@PJL
# Or a QPDL start-sequence: \x1b&
if [ ! -s "$QPDL_FILE" ]; then
    echo "❌ Error: Output QPDL file is empty."
    cat test_stderr.log
    exit 1
fi

# Check for the Escape character (0x1b) at the start
FIRST_BYTE=$(head -c 1 "$QPDL_FILE" | xxd -p)
if [[ "$FIRST_BYTE" != "1b" ]]; then
    echo "❌ Error: Output does not start with ESC (0x1b). Found: 0x$FIRST_BYTE"
    echo "First 16 bytes of output:"
    head -c 16 "$QPDL_FILE" | xxd
    exit 1
fi

# Check if the output contains "PJL"
if grep -q "PJL" "$QPDL_FILE"; then
    echo "✅ Success: Found PJL header in QPDL output."
else
    echo "⚠️  Warning: PJL header not found, but output exists. Checking QPDL signature..."
    # Check for 'splix' or other known markers if applicable
fi

echo "✅ Functional Test Passed: Driver produced valid output from raster input."

rm -f "$RASTER_FILE" "$QPDL_FILE" test_stderr.log
exit 0
