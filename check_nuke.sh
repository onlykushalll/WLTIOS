#!/bin/bash
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "=== MARKER ==="
strings "$LOG" | grep -i "UNIQUE_MARKER" | head
echo "=== WLTIOS BOOT ==="
strings "$LOG" | grep "WLTIOS Boot" | head -5
echo "=== EXECVE ==="
strings "$LOG" | grep "execve" | head -5
echo "=== TOR-PD ==="
strings "$LOG" | grep "tor-pd" | head -5
echo "=== VM-PS ==="
strings "$LOG" | grep "VM.-PS" | head -5
echo "=== LAST 15 ==="
strings "$LOG" | tail -15
