#!/bin/bash
# WLTIOS Section A: Build vm1's Tor rootfs via SAFE cpio APPEND (not extract+repack).
# - Copies the pre-built musl-i386 rootfs (already has Tor + torrc + musl libs)
# - APPENDS only the S99tor init script (cpio -oA) — never extracts, so symlinks preserved
# - Verifies Tor + deps + init symlink intact
set -e
SRC=/home/kushal/wltios-build/rootfs-wlt-full.cpio
APPDIR=/tmp/tor-overlay
OUT=/home/kushal/wltios-build/rootfs_vm1_tor.cpio

echo "[A.1/A.3] Building vm1 Tor rootfs from $SRC (safe append, no extract)"

# Step 1: verify the source rootfs has Tor + all 9 deps BEFORE we build
echo "--- verify source rootfs has Tor + deps ---"
cd /tmp && rm -rf rfs-verify && mkdir rfs-verify && cd rfs-verify
cpio -idm < "$SRC" 2>/dev/null
TOR_ARCH=$(file usr/bin/tor 2>/dev/null)
echo "tor: $TOR_ARCH"
if ! echo "$TOR_ARCH" | grep -q "ld-musl-i386"; then
  echo "FATAL: source rootfs Tor is not musl-i386"; exit 1
fi
MISSING=0
for lib in libz.so.1 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.musl-x86.so.1 ld-musl-i386.so.1; do
  f=$(find . -name "$lib" 2>/dev/null | head -1)
  if [ -z "$f" ]; then echo "MISSING: $lib"; MISSING=1; else echo "OK: $lib -> $f"; fi
done
[ "$MISSING" = "0" ] || { echo "FATAL: missing Tor deps"; exit 1; }

# Step 2: copy source to output
cp "$SRC" "$OUT"
echo "--- copied to $OUT ($(stat -c%s "$OUT") bytes) ---"

# Step 3: build overlay with ONLY S99tor init script
rm -rf "$APPDIR"
mkdir -p "$APPDIR/etc/init.d"
cat > "$APPDIR/etc/init.d/S99tor" <<'TORINIT'
#!/bin/sh
### BEGIN INIT INFO
# Provides:          tor
# Required-Start:    $network
# Required-Stop:
# Default-Start:     2 3 4 5
# Default-Stop:
# Short-Description: Start Tor daemon (TorPD vm1)
### END INIT INFO
# WLTIOS TorPD — Tor auto-start for vm1 only.
# This script exists ONLY in vm1's rootfs (rootfs_vm1.cpio), proving
# genuine per-VM isolation: vm0's rootfs.cpio has no Tor, no S99tor.
case "$1" in
  start)
    echo "[tor-pd] Starting Tor daemon in vm1..."
    mkdir -p /var/lib/tor /var/run
    /usr/bin/tor -f /etc/tor/torrc > /tmp/tor.log 2>&1 &
    echo $! > /var/run/tor.pid
    echo "[tor-pd] Tor started (pid=$(cat /var/run/tor.pid 2>/dev/null))"
    ;;
  stop)
    kill "$(cat /var/run/tor.pid 2>/dev/null)" 2>/dev/null
    ;;
  status)
    if [ -f /var/run/tor.pid ] && kill -0 "$(cat /var/run/tor.pid)" 2>/dev/null; then
      echo "[tor-pd] Tor is running (pid=$(cat /var/run/tor.pid))"
    else
      echo "[tor-pd] Tor is NOT running"
    fi
    ;;
  *)
    echo "Usage: $0 {start|stop|status}"
    ;;
esac
exit 0
TORINIT
chmod +x "$APPDIR/etc/init.d/S99tor"

# Step 4: APPEND overlay to rootfs. -H newc matches source format. -A = append.
( cd "$APPDIR" && find . -print0 | cpio -o0A -H newc -O "$OUT" 2>/dev/null )
echo "[A.1/A.3] Done. Output: $OUT ($(stat -c%s "$OUT") bytes)"

# Step 5: verify S99tor is in archive + init symlink preserved
echo "--- verify S99tor in archive ---"
cpio -itv < "$OUT" 2>/dev/null | grep "S99tor"
echo "--- verify init entry preserved (not corrupted) ---"
cpio -itv < "$OUT" 2>/dev/null | grep -E "^[^d].*\binit$" | head -3
echo "--- verify Tor still present ---"
cpio -itv < "$OUT" 2>/dev/null | grep "usr/bin/tor$"
echo "--- verify musl libc still present ---"
cpio -itv < "$OUT" 2>/dev/null | grep "ld-musl-i386" | head -2
echo "[A.1/A.3] ALL VERIFIED"
