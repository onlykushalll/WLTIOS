#!/bin/bash
# WLTIOS Build Script
# Builds the entire OS: seL4 + Linux guest + WLTIOS userspace
#
# Usage:
#   ./scripts/build.sh              # Full build
#   ./scripts/build.sh --clean      # Clean build
#   ./scripts/build.sh --wlt-boot   # Build only WLT-Boot
#   ./scripts/build.sh --wlt-init   # Build only WLT-Init
#   ./scripts/build.sh --sel4       # Build only seL4 + Linux

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
SEL4_BUILD_DIR="$PROJECT_ROOT/sel4-build/build-minimal-64"

# MSYS2 paths (for Windows)
export PATH="/mingw64/bin:/usr/bin:/bin:$PATH"

print_ok()   { echo -e "${GREEN}[OK]${NC} $1"; }
print_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_err()  { echo -e "${RED}[ERROR]${NC} $1"; }

# Parse args
CLEAN=false
BUILD_WLT_BOOT=false
BUILD_WLT_INIT=false
BUILD_SEL4=false

for arg in "$@"; do
    case $arg in
        --clean)    CLEAN=true ;;
        --wlt-boot) BUILD_WLT_BOOT=true ;;
        --wlt-init) BUILD_WLT_INIT=true ;;
        --sel4)     BUILD_SEL4=true ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

# If no specific target, build all
if [ "$BUILD_WLT_BOOT" = false ] && [ "$BUILD_WLT_INIT" = false ] && [ "$BUILD_SEL4" = false ]; then
    BUILD_WLT_BOOT=true
    BUILD_WLT_INIT=true
    BUILD_SEL4=true
fi

mkdir -p "$BUILD_DIR"

# ─── seL4 + Linux Build ────────────────────────────────
if [ "$BUILD_SEL4" = true ]; then
    echo "=== Building seL4 + Linux Guest ==="
    
    if [ "$CLEAN" = true ]; then
        echo "Cleaning seL4 build..."
        rm -rf "$SEL4_BUILD_DIR"
    fi
    
    if [ ! -d "$SEL4_BUILD_DIR" ]; then
        mkdir -p "$SEL4_BUILD_DIR"
        cd "$SEL4_BUILD_DIR"
        sh "$PROJECT_ROOT/sel4-build/init-build.sh" \
            -DCAMKES_VM_APP=minimal_64 \
            -DPLATFORM=pc99
    else
        cd "$SEL4_BUILD_DIR"
    fi
    
    ninja
    print_ok "seL4 + Linux built"
    
    # Copy image to build dir
    cp images/* "$BUILD_DIR/" 2>/dev/null || print_warn "No images found"
fi

# ─── WLT-Boot Build ────────────────────────────────────
if [ "$BUILD_WLT_BOOT" = true ]; then
    echo "=== Building WLT-Boot ==="
    cd "$PROJECT_ROOT/src/boot/wlt-boot"
    
    if [ "$CLEAN" = true ]; then
        make clean
    fi
    
    make
    print_ok "WLT-Boot built"
    
    cp wlt-boot.efi "$BUILD_DIR/" 2>/dev/null || print_warn "WLT-Boot binary not found"
fi

# ─── WLT-Init Build ────────────────────────────────────
if [ "$BUILD_WLT_INIT" = true ]; then
    echo "=== Building WLT-Init ==="
    cd "$PROJECT_ROOT/src/userspace/wlt-init"
    
    if [ "$CLEAN" = true ]; then
        make clean
    fi
    
    make
    print_ok "WLT-Init built"
    
    cp wlt-init "$BUILD_DIR/" 2>/dev/null || print_warn "WLT-Init binary not found"
fi

echo ""
print_ok "Build complete!"
echo "Build artifacts in: $BUILD_DIR"
ls -la "$BUILD_DIR/"
