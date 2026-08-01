#!/bin/bash
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "===WLTIOS-INIT-MESSAGES==="
strings "$LOG" | grep -iE "WLTIOS|wlt-shell|wlt-init|tor-pd|VM.-PS|vm.-check|Starting|Shell" | head -30
echo ""
echo "===LAST 30 STRINGS==="
strings "$LOG" | tail -30
