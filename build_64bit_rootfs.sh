#!/bin/bash
# A.1-A.3 FIXED: Build 64-bit rootfs with Tor for vm1.
# Base: rootfs-wlt.cpio (64-bit, has busybox + init, works with 64-bit kernel)
# Append: Tor + deps + torrc from wltios-rootfs64/ directory
# Append: S99tor + S99zpscheck init scripts
set -e

BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
APPDIR=/tmp/tor64-overlay

echo "=== Verify base rootfs is 64-bit ==="
cd /tmp && rm -rf verify-base && mkdir verify-base && cd verify-base
cpio -idm < "$BASE" 2>/dev/null
echo "busybox: $(file bin/busybox 2>/dev/null)"
if ! file bin/busybox 2>/dev/null | grep -q "x86-64"; then
  echo "FATAL: base rootfs busybox is not x86-64"; exit 1
fi
echo "init: $(file init 2>/dev/null)"
echo "Base rootfs OK (64-bit)"

echo ""
echo "=== Verify Tor deps in wltios-rootfs64 ==="
cd "$TOR_SRC"
MISSING=0
for lib in libz.so.1 libm.so.6 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 libsystemd.so.0 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.so.6 ld-linux-x86-64.so.2; do
  f=$(find lib lib64 -name "$lib" 2>/dev/null | head -1)
  if [ -z "$f" ]; then echo "MISSING: $lib"; MISSING=1; else echo "OK: $lib -> $f"; fi
done
[ "$MISSING" = "0" ] || { echo "FATAL: missing Tor deps"; exit 1; }
echo "Tor: $(file usr/bin/tor)"
echo "All Tor deps present"

echo ""
echo "=== Build vm1 rootfs (64-bit + Tor) ==="
cp "$BASE" "$VM1_OUT"
rm -rf "$APPDIR"; mkdir -p "$APPDIR"

# Copy Tor binary + torrc + geoip + all deps
mkdir -p "$APPDIR/usr/bin" "$APPDIR/etc/tor" "$APPDIR/usr/share/tor" "$APPDIR/var/lib/tor" "$APPDIR/lib" "$APPDIR/lib64"
cp "$TOR_SRC/usr/bin/tor" "$APPDIR/usr/bin/tor"
cp "$TOR_SRC/etc/tor/torrc" "$APPDIR/etc/tor/torrc" 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* "$APPDIR/usr/share/tor/" 2>/dev/null || true

# Copy all Tor shared lib deps (follow symlinks)
for lib in libz.so.1 libm.so.6 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 libsystemd.so.0 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.so.6 ld-linux-x86-64.so.2; do
  # Find the actual file (not symlink) and copy it + symlinks
  for f in $(find "$TOR_SRC/lib" "$TOR_SRC/lib64" -name "$lib*" 2>/dev/null); do
    rel="${f#$TOR_SRC/}"
    mkdir -p "$APPDIR/$(dirname "$rel")"
    cp -P "$f" "$APPDIR/$rel" 2>/dev/null || true
  done
done

# S99tor init script
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
case "$1" in
  start)
    echo "[tor-pd] Starting Tor daemon in vm1..."
    mkdir -p /var/lib/tor /var/run /tmp
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

# S99zpscheck - prints vm1's process list (runs after S99tor)
cat > "$APPDIR/etc/init.d/S99zpscheck" <<'PSCHECK1'
#!/bin/sh
case "$1" in
  start)
    sleep 3
    echo "=====VM1-PS-BEGIN====="
    ps aux 2>/dev/null || ps
    echo "=====VM1-PS-END====="
    echo "[vm1-check] tor present: $(ps 2>/dev/null | grep -c '[t]or')"
    echo "[vm1-check] tor log tail:"
    tail -5 /tmp/tor.log 2>/dev/null || echo "(no tor log)"
    ;;
esac
exit 0
PSCHECK1
chmod +x "$APPDIR/etc/init.d/S99zpscheck"

( cd "$APPDIR" && find . -print0 | cpio -o0A -H newc -O "$VM1_OUT" 2>/dev/null )
echo "vm1 rootfs: $VM1_OUT ($(stat -c%s "$VM1_OUT") bytes)"

echo ""
echo "=== Build vm0 rootfs (64-bit, NO Tor) ==="
cp "$BASE" "$VM0_OUT"
rm -rf "$APPDIR"; mkdir -p "$APPDIR/etc/init.d"
cat > "$APPDIR/etc/init.d/S98pscheck" <<'PSCHECK0'
#!/bin/sh
case "$1" in
  start)
    sleep 3
    echo "=====VM0-PS-BEGIN====="
    ps aux 2>/dev/null || ps
    echo "=====VM0-PS-END====="
    echo "[vm0-check] tor present: $(ps 2>/dev/null | grep -c '[t]or')"
    ;;
esac
exit 0
PSCHECK0
chmod +x "$APPDIR/etc/init.d/S98pscheck"
( cd "$APPDIR" && find . -print0 | cpio -o0A -H newc -O "$VM0_OUT" 2>/dev/null )
echo "vm0 rootfs: $VM0_OUT ($(stat -c%s "$VM0_OUT") bytes)"

echo ""
echo "=== Verify ==="
echo "--- vm1 (has tor) ---"
cpio -itv < "$VM1_OUT" 2>/dev/null | grep -E "S99tor|S99zpscheck|usr/bin/tor$" | head
echo "--- vm0 (no tor) ---"
cpio -itv < "$VM0_OUT" 2>/dev/null | grep -E "S98pscheck|usr/bin/tor$" | head
echo "--- init preserved in both ---"
cpio -itv < "$VM1_OUT" 2>/dev/null | grep -E "^[^d].*\binit$" | head -2
cpio -itv < "$VM0_OUT" 2>/dev/null | grep -E "^[^d].*\binit$" | head -2
echo "[DONE] Both 64-bit rootfs built"
