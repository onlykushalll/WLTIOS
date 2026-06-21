#!/bin/bash
# WLTIOS QEMU Test Script
# Boots WLTIOS in QEMU and verifies it starts correctly
#
# Usage:
#   ./scripts/qemu-test.sh              # Boot and wait for user
#   ./scripts/qemu-test.sh --no-wait    # Boot, check, exit (automated)
#   ./scripts/qemu-test.sh --debug      # Boot with GDB server

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# QEMU binary
QEMU="qemu-system-x86_64"

# Check QEMU is available
if ! command -v $QEMU > /dev/null 2>&1; then
    # Try Windows path
    if [ -f "/c/Program Files/qemu/qemu-system-x86_64.exe" ]; then
        QEMU="/c/Program Files/qemu/qemu-system-x86_64.exe"
    else
        echo -e "${RED}[ERROR]${NC} QEMU not found"
        exit 1
    fi
fi

# Parse args
NO_WAIT=false
DEBUG=false
for arg in "$@"; do
    case $arg in
        --no-wait) NO_WAIT=true ;;
        --debug)   DEBUG=true ;;
    esac
done

# Find the OS image
IMAGE=""
if [ -f "$BUILD_DIR/wltios.iso" ]; then
    IMAGE="$BUILD_DIR/wltios.iso"
    BOOT_MODE="-cdrom $IMAGE"
elif [ -f "$SEL4_BUILD_DIR/images/kernel" ]; then
    IMAGE="$SEL4_BUILD_DIR/images/kernel"
    BOOT_MODE="-kernel $IMAGE"
elif [ -f "$BUILD_DIR/images/kernel" ]; then
    IMAGE="$BUILD_DIR/images/kernel"
    BOOT_MODE="-kernel $IMAGE"
else
    echo -e "${RED}[ERROR]${NC} No WLTIOS image found"
    echo "Build the OS first: ./scripts/build.sh"
    exit 1
fi

echo -e "${GREEN}[INFO]${NC} Booting WLTIOS in QEMU"
echo "  Image: $IMAGE"
echo "  QEMU:  $QEMU"
echo ""

# QEMU arguments
QEMU_ARGS=(
    -enable-kvm                    # Hardware acceleration (if available)
    -cpu host                      # Use host CPU
    -m 2G                          # 2GB RAM
    -smp 2                         # 2 CPU cores
    -serial mon:stdio              # Serial console
    -display none                  # No display (headless for CI)
    -no-reboot                     # Exit on reboot
    $BOOT_MODE
)

# Debug mode: add GDB server
if [ "$DEBUG" = true ]; then
    QEMU_ARGS+=(-s -S)  # -s: GDB on port 1234, -S: pause at start
    echo -e "${YELLOW}[INFO]${NC} GDB server on port 1234 (paused at start)"
    echo "Connect with: gdb -ex 'target remote localhost:1234'"
fi

# Boot
if [ "$NO_WAIT" = true ]; then
    # Automated: boot, wait 30s, check serial output for success
    echo -e "${YELLOW}[INFO]${NC} Automated mode (30s timeout)"
    timeout 30 $QEMU "${QEMU_ARGS[@]}" 2>&1 | tee /tmp/wlt-qemu-output.log || true
    
    # Check for success indicators
    if grep -qi "seL4" /tmp/wlt-qemu-output.log; then
        echo -e "${GREEN}[PASS]${NC} seL4 boot message found"
        exit 0
    else
        echo -e "${RED}[FAIL]${NC} seL4 boot message not found"
        exit 1
    fi
else
    # Interactive: boot and wait for user
    QEMU_ARGS+=(-display gtk)  # Show window for interactive
    echo -e "${YELLOW}[INFO]${NC} Interactive mode (Ctrl+A X to exit)"
    $QEMU "${QEMU_ARGS[@]}"
fi
