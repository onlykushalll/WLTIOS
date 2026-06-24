#!/bin/bash
# WLTIOS QEMU boot with network + Tor support
# Usage: ./scripts/qemu-boot-net.sh [timeout_seconds]
#
# Boots WLTIOS in QEMU with virtio-net-device for user-mode networking.
# Default timeout: 60 seconds (enough for Tor bootstrap + shell commands)
#
# Network: QEMU user-mode networking
#   - Guest IP:   10.0.2.15 (automatic via our wlt-net bringup)
#   - Gateway:    10.0.2.2
#   - DNS:        10.0.2.3
#   - Outbound:   TCP/UDP/ICMP to any host (NAT)

set -e

BUILD_DIR="$HOME/wltios-build/build-minimal-32"
INITRAMFS="$HOME/wltios-build/wltios-initramfs.cpio.gz"
KERNEL="$BUILD_DIR/images/kernel-ia32-pc99"
INITRD_BASE="$BUILD_DIR/images/capdl-loader-image-ia32-pc99"

TIMEOUT=${1:-60}
OUTPUT_FILE="/tmp/wltios-net-boot.txt"

# Verify images exist
if [ ! -f "$KERNEL" ]; then
    echo "ERROR: seL4 kernel not found at $KERNEL"
    exit 1
fi
if [ ! -f "$INITRD_BASE" ]; then
    echo "ERROR: capdl-loader image not found at $INITRD_BASE"
    exit 1
fi
if [ ! -f "$INITRAMFS" ]; then
    echo "ERROR: initramfs not found at $INITRAMFS"
    echo "Build it first: cd ~/wltios-build && bash /mnt/c/.../scripts/build-initramfs.sh"
    exit 1
fi

echo "=== WLTIOS QEMU Boot (Network + Tor) ==="
echo "  Kernel:    $KERNEL"
echo "  Initrd:    $INITRD_BASE"
echo "  Initramfs: $INITRAMFS ($(ls -lh $INITRAMFS | awk '{print $5}'))"
echo "  Timeout:   ${TIMEOUT}s"
echo "  Network:   virtio-net (user-mode, 10.0.2.15/24, gw 10.0.2.2)"
echo "  Output:    $OUTPUT_FILE"
echo ""

# Boot QEMU with virtio-net-device for user-mode networking
# The initramfs is loaded via the -initrd flag (replaces the capdl-loader embedded one)
# Actually: seL4 uses capdl-loader-image as the initrd. Our custom initramfs is
# embedded INSIDE the capdl-loader image during the seL4 build. So we need to
# rebuild seL4 after updating the rootfs.
#
# For now, we pass the initramfs separately as a second initrd. QEMU supports
# multiple initrds via comma-separated -initrd flag, but seL4's capdl-loader
# expects a specific format. The proper way is:
#   1. Build initramfs with build-initramfs.sh
#   2. Inject into seL4's rootfs.cpio
#   3. Rebuild seL4 (ninja)
#   4. Boot with the rebuilt capdl-loader-image
#
# This script assumes step 3 has been done. If not, run:
#   ./scripts/inject-and-rebuild.sh

timeout $TIMEOUT qemu-system-i386 \
    -machine pc \
    -cpu host \
    -m 512M \
    -enable-kvm \
    -kernel "$KERNEL" \
    -initrd "$INITRD_BASE" \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-device,netdev=net0 \
    -nographic \
    -append "console=ttyS0 earlycon=uart8250,io,0x3f8 init=/sbin/init" \
    2>&1 | tee "$OUTPUT_FILE" || true

echo ""
echo "=== Boot complete. Output saved to $OUTPUT_FILE ==="
echo ""
echo "=== Verification ==="
echo "Network bringup:"
grep -a "wlt-net" "$OUTPUT_FILE" | head -20
echo ""
echo "Tor bootstrap:"
grep -ai "Bootstrapped" "$OUTPUT_FILE" | head -20
echo ""
echo "Shell prompt:"
grep -a "wlt#" "$OUTPUT_FILE" | head -5