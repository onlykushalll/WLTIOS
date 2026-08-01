#!/bin/bash
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "===KEY MESSAGES==="
strings "$LOG" | grep -iE "Linux version|Welcome|login:|panic|init|binfmt|RAMDISK|rootfs|Freeing|execute|error|tor-pd|VM0-PS|VM1-PS|Booting|shell|cpio" | head -60
echo ""
echo "===LAST 20 LINES==="
strings "$LOG" | tail -20
