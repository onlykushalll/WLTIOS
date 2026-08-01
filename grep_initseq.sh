#!/bin/bash
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "=== AROUND INIT EXECUTION ==="
strings "$LOG" | grep -n -A5 "Freeing unused kernel memory: 660K" | head -30
echo ""
echo "=== FAILED TO EXECUTE ==="
strings "$LOG" | grep -i "failed to execute\|couldn't execute\|No working init\|execve failed\|error -8" | head -10
echo ""
echo "=== WLTIOS BOOT CONTEXT ==="
strings "$LOG" | grep -B2 -A2 "WLTIOS Boot" | head -20
