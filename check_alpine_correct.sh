#!/bin/bash
LOG=/home/kushal/torpd-alpine-correct.log
echo "=== LOG ==="
ls -la "$LOG" 2>/dev/null
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== Tor + deps ==="
file /tmp/alpine-correct/extracted/usr/bin/tor 2>/dev/null
cd /tmp/alpine-correct/extracted 2>/dev/null
for lib in libz.so.1 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.musl-x86_64.so.1; do
    f=$(find . -name "$lib" 2>/dev/null | head -1)
    if [ -z "$f" ]; then echo "MISSING: $lib"; else echo "OK: $lib -> $f"; fi
done
echo ""
echo "=== Boot results ==="
if [ -f "$LOG" ]; then
    strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check\|Starting Tor\|segfault\|relocating\|RESULT\|bridge" | head -20
    echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
fi
