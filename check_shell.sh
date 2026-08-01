#!/bin/bash
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "=== LAST 30 LINES ==="
strings "$LOG" | tail -30
echo ""
echo "=== WLTIOS BOOT MESSAGES ==="
strings "$LOG" | grep "WLTIOS Boot" | head -5
echo ""
echo "=== EXECVE ERRORS ==="
strings "$LOG" | grep -i "execve" | head -5
echo ""
echo "=== SHELL PROMPTS ==="
strings "$LOG" | grep -E "#|\\$" | grep -v "^\[" | tail -10
