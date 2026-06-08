#!/bin/bash
# Greeny Temp Controller — Auto-ISP via USB CDC !ISP command
#
# First flash (blank chip): hold P3.2 button, plug USB, run ./burn.sh --first
# All subsequent flashes: ./burn.sh (fully automatic)
#
# Usage: ./burn.sh [--first]

set -e

FIRST="${1}"
FW="thermo.ihx"

if [ ! -f "$FW" ]; then
    echo "Firmware not found: $FW"
    echo "Run 'make' first."
    exit 1
fi

# Detect STC8H8K64U USB bootloader device
find_bootloader() {
    # STC USB ISP bootloader: USB VID:PID = 0x1CBE:0x00FF or similar
    # On Mac, it typically shows up in system_profiler
    system_profiler SPUSBDataType 2>/dev/null \
        | grep -A5 "STC" | grep "Serial Number" | head -1 | awk '{print $NF}'
}

if [ "$FIRST" = "--first" ]; then
    echo "=== First-time flash (blank chip) ==="
    echo "Make sure P3.2 button is HELD to GND, then USB is connected."
    echo "Waiting for STC USB Bootloader..."
    echo ""

    # Wait for bootloader to appear (max 10 seconds)
    for i in $(seq 1 20); do
        SN=$(find_bootloader)
        if [ -n "$SN" ]; then
            echo "Bootloader found: $SN"
            break
        fi
        sleep 0.5
    done

    if [ -z "$SN" ]; then
        echo "Timeout: STC USB Bootloader not found."
        echo "Check: hold P3.2 button → plug USB → release button → retry."
        exit 1
    fi

    # Flash via USB ISP
    echo "Flashing..."
    stc8prog -d "$SN" "$FW"
    echo "Done! MCU will reboot into new firmware with USB CDC."

else
    echo "=== Auto-ISP via USB CDC ==="

    # Find the CDC device
    CDC=$(ls /dev/tty.usbmodem* 2>/dev/null | head -1)
    if [ -z "$CDC" ]; then
        echo "USB CDC device not found at /dev/tty.usbmodem*"
        echo "Is the MCU running and USB connected?"
        exit 1
    fi
    echo "CDC device: $CDC"

    # Send !ISP command → MCU reboots to USB bootloader
    echo "Sending !ISP command..."
    stty -f "$CDC" 9600
    echo -ne "!ISP\r\n" > "$CDC"
    sleep 2  # Wait for MCU to reboot into bootloader

    # Wait for bootloader to enumerate
    echo "Waiting for bootloader..."
    for i in $(seq 1 20); do
        SN=$(find_bootloader)
        if [ -n "$SN" ]; then
            echo "Bootloader: $SN"
            break
        fi
        sleep 0.5
    done

    if [ -z "$SN" ]; then
        echo "STC USB Bootloader not found after !ISP."
        echo "Falling back: try '--first' mode (hold P3.2, replug USB, ./burn.sh --first)"
        exit 1
    fi

    # Flash via USB ISP
    echo "Flashing..."
    stc8prog -d "$SN" "$FW"

    echo ""
    echo "=== Done! New firmware running. ==="
fi
