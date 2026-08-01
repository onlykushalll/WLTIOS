#!/bin/bash
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "===SECTIONS==="
echo "--- seL4 boot (first 15 lines after boot config) ---"
grep -n "Booting\|vm0\|vm1\|VM_\|guest\|initrd\|rootfs\|bzimage\|kernel_image" "$LOG" 2>/dev/null | strings | head -30
echo ""
echo "--- Linux kernel version ---"
grep "Linux version" "$LOG" 2>/dev/null | strings | head -3
echo ""
echo "--- rootfs/init messages ---"
grep -n "init\|rootfs\|cpio\|RAMDISK\|Freeing\|execute" "$LOG" 2>/dev/null | strings | head -20
echo ""
echo "--- error -8 context ---"
grep -n -B1 -A1 "error -8\|ENOEXEC\|binfmt" "$LOG" 2>/dev/null | strings | head -20
echo ""
echo "--- check if BOTH VMs booted ---"
grep -c "Linux version" "$LOG" 2>/dev/null
echo "(count of Linux version strings = number of VM kernels booted)"
