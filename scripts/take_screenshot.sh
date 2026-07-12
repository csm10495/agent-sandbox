#!/usr/bin/env bash
# scripts/take_screenshot.sh
# Boots os.iso in QEMU and captures the VGA framebuffer as docs/screenshot.png
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO="$REPO_ROOT/os.iso"
PPM="$REPO_ROOT/docs/screen.ppm"
PNG="$REPO_ROOT/docs/screenshot.png"
BOOT_WAIT=18   # seconds to let OS boot before screenshot

mkdir -p "$REPO_ROOT/docs"

if [ ! -f "$ISO" ]; then
    echo "ERROR: $ISO not found." >&2
    exit 1
fi

echo "Starting QEMU for screenshot (wait ${BOOT_WAIT}s)..."

# QEMU: display=none but keeps VGA framebuffer; monitor via stdio with delay
(
    sleep "$BOOT_WAIT"
    echo "screendump $PPM"
    sleep 2
    echo "quit"
) | timeout $((BOOT_WAIT + 10)) \
    qemu-system-x86_64 \
        -cdrom "$ISO" \
        -m 128M \
        -display none \
        -vga std \
        -serial "file:$REPO_ROOT/screenshot_serial.log" \
        -monitor stdio \
        -no-reboot \
    2>/dev/null || true

if [ -f "$PPM" ]; then
    echo "PPM captured: $PPM"
    # Convert PPM to PNG
    if command -v convert &>/dev/null; then
        convert "$PPM" "$PNG"
        rm -f "$PPM"
        echo "Screenshot saved: $PNG"
    else
        mv "$PPM" "${PNG%.png}.ppm"
        echo "Screenshot saved (PPM): ${PNG%.png}.ppm"
    fi
else
    echo "WARNING: screendump did not produce output." >&2
    # Fall back: use serial output as text screenshot
    if [ -f "$REPO_ROOT/screenshot_serial.log" ]; then
        echo "Serial log available at $REPO_ROOT/screenshot_serial.log"
    fi
fi

rm -f "$REPO_ROOT/screenshot_serial.log"
