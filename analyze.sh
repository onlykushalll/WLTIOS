#!/bin/bash
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "===TOR-PD-MESSAGES==="
grep "tor-pd" "$LOG" 2>/dev/null | head -10
echo "===VM0-PS==="
sed -n '/VM0-PS-BEGIN/,/VM0-PS-END/p' "$LOG" 2>/dev/null | head -25
echo "===VM1-PS==="
sed -n '/VM1-PS-BEGIN/,/VM1-PS-END/p' "$LOG" 2>/dev/null | head -25
echo "===VM-CHECK==="
grep "vm.-check" "$LOG" 2>/dev/null
echo "===SUMMARY==="
echo "Welcome=$(grep -c 'Welcome to Buildroot' "$LOG" 2>/dev/null)"
echo "login=$(grep -c 'login:' "$LOG" 2>/dev/null)"
echo "VM_FATAL=$(grep -c 'VM_FATAL' "$LOG" 2>/dev/null)"
echo "panic=$(grep -ci 'kernel panic' "$LOG" 2>/dev/null)"
echo "binfmt464c=$(grep -c 'binfmt-464c' "$LOG" 2>/dev/null)"
echo "ISA=$(grep -c 'CPU ISA' "$LOG" 2>/dev/null)"
