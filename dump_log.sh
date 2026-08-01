#!/bin/bash
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "===FIRST 40 LINES==="
head -40 "$LOG" | strings | head -40
echo "===PANIC CONTEXT==="
grep -n -A2 -B2 "panic\|binfmt-464c\|VM_FATAL\|Booting vm\|@start\|init=" "$LOG" 2>/dev/null | strings | head -60
echo "===LAST 30 LINES==="
tail -30 "$LOG" | strings | head -30
