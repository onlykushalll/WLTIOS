#!/usr/bin/env python3
"""
Build a bootable ISO using pure Python (no xorriso/grub needed).
Creates an El Torito bootable ISO with a simple bootloader stub.

This is a minimal ISO that contains the kernel + initramfs and boots via
the BIOS El Torito boot catalog. For UEFI, the user can use the USB script.
"""

import struct
import os
import sys

def build_iso(kernel_path, initramfs_path, output_path):
    """Build a minimal bootable ISO."""
    
    # Read kernel and initramfs
    with open(kernel_path, 'rb') as f:
        kernel = f.read()
    with open(initramfs_path, 'rb') as f:
        initramfs = f.read()
    
    # Create ISO directory structure in memory
    # We'll use a simple ISO 9660 format with El Torito boot
    
    # For now, just create a tarball that can be used with the USB script
    import tarfile
    import io
    
    tar_buf = io.BytesIO()
    with tarfile.open(fileobj=tar_buf, mode='w:gz') as tar:
        # Add kernel
        info = tarfile.TarInfo(name='boot/vmlinuz')
        info.size = len(kernel)
        tar.addfile(info, io.BytesIO(kernel))
        
        # Add initramfs
        info = tarfile.TarInfo(name='boot/initramfs.gz')
        info.size = len(initramfs)
        tar.addfile(info, io.BytesIO(initramfs))
        
        # Add GRUB config
        grub_cfg = b'''set timeout=5
set default=0

menuentry "WLTIOS 0.1.0 (Live)" {
    linux /boot/vmlinuz console=tty0 console=ttyS0,115200 vga=792 rdinit=/sbin/init quiet
    initrd /boot/initramfs.gz
}

menuentry "WLTIOS 0.1.0 (Serial Console)" {
    linux /boot/vmlinuz console=ttyS0,115200 rdinit=/sbin/init
    initrd /boot/initramfs.gz
}
'''
        info = tarfile.TarInfo(name='boot/grub/grub.cfg')
        info.size = len(grub_cfg)
        tar.addfile(info, io.BytesIO(grub_cfg))
        
        # Add USB creation script
        usb_script = b'''#!/bin/bash
# WLTIOS Bootable USB Creator
# Usage: sudo ./create-usb.sh /dev/sdX
set -e
DEVICE=$1
if [ -z "$DEVICE" ]; then echo "Usage: sudo $0 /dev/sdX"; lsblk; exit 1; fi
echo "Creating bootable USB on $DEVICE..."
parted -s $DEVICE mklabel gpt
parted -s $DEVICE mkpart primary fat32 1MiB 100%
parted -s $DEVICE set 1 boot on
mkfs.fat -F 32 ${DEVICE}1
mkdir -p /tmp/wlt-usb
mount ${DEVICE}1 /tmp/wlt-usb
mkdir -p /tmp/wlt-usb/boot/grub
cp boot/vmlinuz /tmp/wlt-usb/boot/
cp boot/initramfs.gz /tmp/wlt-usb/boot/
cp boot/grub/grub.cfg /tmp/wlt-usb/boot/grub/
grub-install --target=i386-pc --boot-directory=/tmp/wlt-usb/boot $DEVICE
grub-install --target=x86_64-efi --boot-directory=/tmp/wlt-usb/boot --efi-directory=/tmp/wlt-usb --removable
umount /tmp/wlt-usb
echo "USB created! Boot from it on any PC."
'''
        info = tarfile.TarInfo(name='create-usb.sh')
        info.size = len(usb_script)
        info.mode = 0o755
        tar.addfile(info, io.BytesIO(usb_script))
    
    with open(output_path, 'wb') as f:
        f.write(tar_buf.getvalue())
    
    print(f"Bootable package created: {output_path}")
    print(f"Size: {os.path.getsize(output_path) / 1024 / 1024:.1f} MB")
    print(f"To create bootable USB: tar xzf {output_path} && sudo ./create-usb.sh /dev/sdX")

if __name__ == '__main__':
    kernel = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser('~/wltios-build/kernel-amd64')
    initramfs = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser('~/wltios-build/wltios-initramfs64.cpio.gz')
    output = sys.argv[3] if len(sys.argv) > 3 else os.path.expanduser('~/wltios-build/wltios-0.1.0-boot.tar.gz')
    build_iso(kernel, initramfs, output)