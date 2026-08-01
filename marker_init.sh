#!/bin/sh
# Minimal init: just echo a unique marker to verify /init runs.
echo "UNIQUE_MARKER_INIT_RUNNING_$(date +%s 2>/dev/null || echo NOTIME)" > /dev/console
echo "UNIQUE_MARKER_INIT_STDOUT" 
mount -t proc proc /proc 2>/dev/null
mount -t sysfs sys /sys 2>/dev/null
mount -t devtmpfs dev /dev 2>/dev/null
mount -t tmpfs tmp /tmp 2>/dev/null
# Run init scripts
for s in /etc/init.d/S*; do
  [ -x "$s" ] && "$s" start &
done
sleep 12
echo "UNIQUE_MARKER_INIT_DROPPING_TO_SHELL" > /dev/console
exec setsid /bin/sh < /dev/console > /dev/console 2>&1
