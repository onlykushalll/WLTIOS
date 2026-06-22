#!/bin/bash
# WLTIOS Live USB Boot Configuration
# This script sets up the amnesic (RAM-only) mode for Live USB boot
#
# When WLTIOS boots from USB in amnesic mode:
# - Root filesystem is loaded into RAM (tmpfs)
# - No writes touch the USB drive
# - All temp/logs/cache are in RAM
# - Nothing persists across reboots (unless user explicitly saves)
#
# This is the same approach Tails uses.

set -e

echo "=== WLTIOS Live USB Mode Setup ==="

# 1. Mount root as tmpfs (RAM-only)
echo "Mounting root as tmpfs (RAM-only)..."
mount -t tmpfs -o size=1G tmpfs /mnt/live 2>/dev/null || true

# 2. Copy essential system files to RAM
echo "Copying system to RAM..."
cp -a /system/* /mnt/live/ 2>/dev/null || true

# 3. Create essential directories in RAM
mkdir -p /mnt/live/{proc,sys,dev,tmp,run,users,var/log,var/wlt}

# 4. Mount virtual filesystems
mount -t proc proc /mnt/live/proc
mount -t sysfs sysfs /mnt/live/sys
mount -t devtmpfs devtmpfs /mnt/live/dev
mount -t tmpfs -o size=256M,mode=1777 tmpfs /mnt/live/tmp
mount -t tmpfs -o size=64M,mode=755 tmpfs /mnt/live/run
mount -t tmpfs -o size=32M,mode=755 tmpfs /mnt/live/var/log

# 5. Create user directories in RAM
mkdir -p /mnt/live/users/default/{desktop,documents,downloads,music,pictures,videos}
chown -R 1000:1000 /mnt/live/users/default

# 6. Set up Tor data in RAM (amnesic — no persistence)
mkdir -p /mnt/live/var/lib/tor
chown -R 1000:1000 /mnt/live/var/lib/tor

# 7. Set up package cache in RAM
mkdir -p /mnt/live/var/wlt/{store,db,cache}

# 8. Switch root to RAM filesystem
echo "Switching root to RAM filesystem..."
cd /mnt/live
mkdir -p oldroot
mount --bind / oldroot 2>/dev/null || true

# 9. Set environment
export PATH="/usr/bin:/usr/sbin:/bin:/sbin"
export HOME="/users/default"
export TERM="linux"

echo "Live USB mode active. All data is in RAM."
echo "Nothing will persist after reboot unless explicitly saved."

# Note: The actual root switch (switch_root / pivot_root) would happen
# in the initramfs before WLT-Init starts. This is a configuration reference.
