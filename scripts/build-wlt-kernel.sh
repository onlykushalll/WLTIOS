#!/bin/bash
# build-wlt-kernel.sh — Build the WLTIOS custom Linux kernel
#
# Builds a Linux 5.10.218 kernel with e1000 + virtio_net built-in
# (not as modules) so the initramfs doesn't need any .ko files.
#
# Prerequisites (install once):
#   sudo apt install flex bison libelf-dev
# OR run without sudo by extracting .deb files to ~/local (see below)
#
# Output:
#   ~/wltios-build/kernel-amd64  (replaces the Debian stock kernel)
set -e

export PATH="$HOME/.local/bin:$HOME/local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export BISON_PKGDATADIR="$HOME/local/share/bison"
export M4="$HOME/local/bin/m4"

WORK=~/wltios-build
KSRC=$WORK/linux-5.10.218
KOUT=$WORK/kernel-build
KBZ=$KOUT/arch/x86/boot/bzImage
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
CONFIG=$REPO/packages/kernel/wlt-kernel.config

echo "=== 1. Download kernel source if needed ==="
if [ ! -d "$KSRC" ]; then
    cd $WORK
    if [ ! -f linux-5.10.218.tar.xz ]; then
        echo "Downloading Linux 5.10.218..."
        curl -L --max-time 300 -o linux-5.10.218.tar.xz \
            "https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.10.218.tar.xz"
    fi
    echo "Extracting..."
    tar xf linux-5.10.218.tar.xz
fi

echo "=== 2. Configure ==="
mkdir -p "$KOUT"
cd "$KSRC"

# Patch for GCC 15+ (C23 makes bool/true/false keywords, conflicts with kernel enums)
if ! grep -q "__STDC_VERSION__ >= 202311L" include/linux/stddef.h 2>/dev/null; then
    echo "Patching stddef.h for GCC 15+ compatibility..."
    python3 -c "
with open('include/linux/stddef.h', 'r') as f:
    c = f.read()
c = c.replace(
    'enum {\n\tfalse\t= 0,\n\ttrue\t= 1\n};',
    '#if __STDC_VERSION__ >= 202311L\n/* C23: false/true are keywords */\n#else\nenum {\n\tfalse\t= 0,\n\ttrue\t= 1\n};\n#endif'
)
with open('include/linux/stddef.h', 'w') as f:
    f.write(c)
"
fi

cp "$CONFIG" "$KOUT/.config"
make O="$KOUT" HOSTCFLAGS="-I$HOME/local/include -std=gnu11" HOSTLDFLAGS="-L$HOME/local/lib" olddefconfig 2>&1 | tail -3

echo "=== 3. Verify config ==="
grep "^CONFIG_E1000=" "$KOUT/.config"
grep "^CONFIG_VIRTIO_NET=" "$KOUT/.config"

echo "=== 4. Build (5-10 min) ==="
make O="$KOUT" -j$(nproc) \
    EXTRA_CFLAGS="-std=gnu11" \
    HOSTCFLAGS="-I$HOME/local/include -std=gnu11" \
    HOSTLDFLAGS="-L$HOME/local/lib" \
    bzImage 2>&1 | tail -10

echo "=== 5. Install ==="
if [ -f "$KBZ" ]; then
    cp ~/wltios-build/kernel-amd64 ~/wltios-build/kernel-amd64.debian.bak 2>/dev/null || true
    cp "$KBZ" ~/wltios-build/kernel-amd64
    echo "SUCCESS: new kernel at ~/wltios-build/kernel-amd64 ($(stat -c%s "$KBZ") bytes)"
else
    echo "FAILED: bzImage not found"
    exit 1
fi
