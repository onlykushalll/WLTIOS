#!/usr/bin/env bash
# Guardian dev environment activation script.
# Source this:  source activate.sh
# [fix 2026-09-19] Script-relative. The old version hardcoded
# /home/z/local/mamba and /home/z/guardian -- paths from a previous
# machine layout that do not exist in a fresh checkout. The defender is
# self-contained via local-deps/ (see download_deps.sh); micromamba is
# only an optional fallback, overridable with WLTIOS_MAMBA_ROOT.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export GUARDIAN_ROOT="$SCRIPT_DIR"

if [ -d "$SCRIPT_DIR/local-deps/usr" ]; then
    export GUARDIAN_PREFIX="$SCRIPT_DIR/local-deps/usr"
    export PATH="$GUARDIAN_PREFIX/bin:$PATH"
    echo "[guardian] using vendored local-deps at $GUARDIAN_PREFIX"
else
    MAMBA_ROOT_PREFIX="${WLTIOS_MAMBA_ROOT:-$HOME/local/mamba}"
    GUARDIAN_ENV="$MAMBA_ROOT_PREFIX/envs/guardian"
    if [ ! -d "$GUARDIAN_ENV" ]; then
        echo "ERROR: neither $SCRIPT_DIR/local-deps/usr nor $GUARDIAN_ENV found" >&2
        echo "Run ./download_deps.sh first (vendored deps, recommended)," >&2
        echo "or provide a micromamba guardian env via WLTIOS_MAMBA_ROOT." >&2
        return 1 2>/dev/null || exit 1
    fi
    export GUARDIAN_PREFIX="$GUARDIAN_ENV"
    export PATH="$GUARDIAN_ENV/bin:$PATH"
fi

# Toolchain
export CC=clang
export CXX=clang++
export AR=llvm-ar
export NM=llvm-nm
export RANLIB=llvm-ranlib

# Include / lib search paths
export CFLAGS="-I$GUARDIAN_PREFIX/include -I$GUARDIAN_PREFIX/include-linux -I$GUARDIAN_PREFIX/include-linux/linux -I$GUARDIAN_PREFIX/include-linux/asm-generic -O2 -g -Wall"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-L$GUARDIAN_PREFIX/lib -L$GUARDIAN_PREFIX/lib64"
export PKG_CONFIG_PATH="$GUARDIAN_PREFIX/lib/pkgconfig:$GUARDIAN_PREFIX/lib64/pkgconfig"

echo "[guardian] env active (root: $GUARDIAN_ROOT)"
echo "[guardian]   clang:      $(clang --version 2>/dev/null | head -1 || echo n/a)"
echo "[guardian]   yara:       $(yara --version 2>/dev/null || echo n/a)"
echo "[guardian]   libbpf:     $(ls $GUARDIAN_PREFIX/lib/libbpf.a 2>/dev/null && echo static || echo missing)"
echo "[guardian]   BTF source: /sys/kernel/btf/vmlinux ($(du -h /sys/kernel/btf/vmlinux 2>/dev/null | cut -f1 || echo n/a))"
