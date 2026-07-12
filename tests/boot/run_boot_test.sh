#!/usr/bin/env bash
# tests/boot/run_boot_test.sh
# Functional boot test: boots os.iso in QEMU (headless) and checks serial output.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ISO="$REPO_ROOT/os.iso"
TIMEOUT=45
SERIAL_LOG="$REPO_ROOT/boot_serial.log"
EXPECTED_STRINGS=(
    "MicrOS kernel started"
    "Kernel ready"
)

if [ ! -f "$ISO" ]; then
    echo "ERROR: $ISO not found. Run 'make os.iso' first." >&2
    exit 1
fi

echo "Starting QEMU boot test..."
echo "  ISO: $ISO"
echo "  Timeout: ${TIMEOUT}s"

rm -f "$SERIAL_LOG"

# Run QEMU in background: headless, serial to log file, stop after TIMEOUT
timeout "$TIMEOUT" qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 128M \
    -nographic \
    -serial "file:$SERIAL_LOG" \
    -no-reboot \
    -monitor none \
    -display none \
    2>/dev/null &
QEMU_PID=$!

# Poll for expected strings with 1s intervals
FOUND_ALL=true
for STR in "${EXPECTED_STRINGS[@]}"; do
    FOUND=false
    for _ in $(seq 1 "$TIMEOUT"); do
        if [ -f "$SERIAL_LOG" ] && grep -qF "$STR" "$SERIAL_LOG" 2>/dev/null; then
            echo "  [OK] Found: \"$STR\""
            FOUND=true
            break
        fi
        sleep 1
        # Check if QEMU died
        if ! kill -0 "$QEMU_PID" 2>/dev/null; then break; fi
    done
    if [ "$FOUND" = false ]; then
        echo "  [FAIL] Not found: \"$STR\""
        FOUND_ALL=false
    fi
done

# Clean up QEMU
kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true

echo ""
echo "Serial log (last 30 lines):"
echo "---"
tail -30 "$SERIAL_LOG" 2>/dev/null || echo "(empty)"
echo "---"

if [ "$FOUND_ALL" = true ]; then
    echo "BOOT TEST: PASSED"
    exit 0
else
    echo "BOOT TEST: FAILED"
    exit 1
fi
