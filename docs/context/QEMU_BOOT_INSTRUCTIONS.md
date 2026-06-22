# QEMU Boot Instructions — WLTIOS

> **Milestone achieved:** 2026-06-22 — seL4 → Linux → wlt-init-stub → WLT-Shell
> **Boot output confirmed:** `=== WLTIOS Boot ===` appears in QEMU serial output.

## Prerequisites

- WSL2 Ubuntu with build tools (gcc, cmake, ninja, python3)
- seL4 + Linux built in `~/wltios-build/build-minimal-32/` (32-bit CAmkES VM)
- QEMU installed in WSL2: `sudo apt install qemu-system-x86`
- KVM enabled: `sudo chmod 666 /dev/kvm` (or add user to kvm group)

## Step 1: Compile wlt-init-stub (32-bit, static)

```bash
cd ~/wltios-build
gcc -m32 -Wall -Wextra -Werror -static \
    /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace/wlt-init/wlt-init-stub.c \
    -o wlt-init-stub-32
file wlt-init-stub-32
# Must say: ELF 32-bit LSB executable, Intel i386, statically linked
```

## Step 2: Inject stub into Buildroot rootfs

```bash
# Extract existing rootfs
mkdir -p /tmp/rootfs-mod
cd /tmp/rootfs-mod
rm -rf *
cpio -id < ~/wltios-build/build-minimal-32/apps/x86/minimal/fserv/files/rootfs.cpio

# Replace /sbin/init with our stub
cp ~/wltios-build/wlt-init-stub-32 sbin/init
chmod 755 sbin/init

# Repack
find . | cpio --create --format=newc > ~/wltios-build/rootfs-wlt32.cpio

# Copy over the original
cp ~/wltios-build/rootfs-wlt32.cpio \
   ~/wltios-build/build-minimal-32/apps/x86/minimal/fserv/files/rootfs.cpio
```

## Step 3: Rebuild seL4 image

```bash
cd ~/wltios-build/build-minimal-32
ninja -j$(nproc)
# Verify image was rebuilt:
ls -la images/capdl-loader-image-ia32-pc99
```

## Step 4: Boot in QEMU

```bash
cd ~/wltios-build/build-minimal-32
timeout 45 qemu-system-i386 \
    -machine pc \
    -cpu host \
    -m 512M \
    -enable-kvm \
    -kernel images/kernel-ia32-pc99 \
    -initrd images/capdl-loader-image-ia32-pc99 \
    -nographic \
    -append "console=ttyS0 earlycon=uart8250,io,0x3f8 init=/sbin/init" \
    2>&1 | tee /tmp/wltios-boot.txt
```

## Step 5: Verify boot

```bash
grep -a "WLTIOS\|wlt-shell\|wlt-init" /tmp/wltios-boot.txt
```

Expected output:
```
========================================
=== WLTIOS Boot ===
seL4 -> Linux -> WLT-Init-Stub
========================================
[wlt-init] Starting WLT-Shell (attempt 1/5)...
WLTIOS Shell v1.0
Type 'help' for available commands.
/wlt#
```

## Known Issues

- **Null bytes in output:** The 32-bit stub has some `\0` characters in string
  output (visible as `=\u0000= WLTIOS Boot` in binary mode). This is a minor
  formatting issue in the 32-bit string handling — does not affect functionality.
- **CPU requirement:** Must use `-cpu host` (not `-cpu Nehalem`). The Nehalem
  model lacks XSAVE support which seL4 requires.
- **64-bit doesn't boot in WSL2:** The 64-bit seL4 build requires `vmx-apicv-xapic`
  (APIC Virtualization) which WSL2's nested KVM doesn't expose. Use 32-bit for
  development; 64-bit works on bare metal.
- **Timeout:** The `timeout 45` kills QEMU after 45 seconds. The shell stays
  running until then. For interactive use, remove the timeout and use `-serial
  mon:stdio` instead of `-nographic`.

## Architecture

```
seL4 (kernel-ia32-pc99, 1.1MB)
  └── capdl-loader-image-ia32-pc99 (21MB)
        └── Linux 4.8.16 (32-bit guest VM)
              └── /sbin/init = wlt-init-stub-32 (our code)
                    ├── redirects stdin/stdout/stderr to /dev/console
                    ├── mounts /proc, /sys, /dev
                    ├── prints "=== WLTIOS Boot ==="
                    └── exec /bin/wlt-shell (our custom shell)
                          └── /wlt# prompt
```
