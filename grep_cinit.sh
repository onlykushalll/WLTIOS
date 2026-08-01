#!/bin/bash
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "=== SEARCH FOR C INIT MESSAGES ==="
strings "$LOG" | grep -iE "vm init|init started|Dropping to|tor-pd|VM.-PS|vm.-check|Running init" | head -20
echo ""
echo "=== ALL 'init' MESSAGES ==="
strings "$LOG" | grep -i "init" | head -30
echo ""
echo "=== ALL UNIQUE MESSAGES (sorted) ==="
strings "$LOG" | grep -v "^\[" | sort -u | head -40
