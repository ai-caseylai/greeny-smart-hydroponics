#!/bin/bash
# Greeny Temp Controller — CH582 USB direct flash via wchisp
#
# First time: hold BOOT button (PA9 to GND), plug USB, run ./flash.sh
# After that: just run ./flash.sh (auto via USB bootloader)
#
# Install: pip3 install wchisp
#
# Usage: ./flash.sh [firmware.bin]

set -e

FW="${1:-wristband.bin}"

if [ ! -f "$FW" ]; then
    echo "Firmware not found: $FW"
    echo "Run 'make' first."
    exit 1
fi

echo "=== CH582 USB Flash ==="
echo "Firmware: $FW"
echo ""

# wchisp auto-detects the CH582 USB bootloader
wchisp flash "$FW"

echo ""
echo "=== Done! MCU will reboot into new firmware. ==="
