#!/bin/bash
# Rebuild both rootfs with: fixed init (use /bin/sh not 32-bit wlt-shell) + ps-check + Tor (vm1 only)
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
APPDIR=/tmp/fixed-overlay

# Fixed init script: uses /bin/sh (busybox 64-bit) instead of /bin/wlt-shell (32-bit)
FIXED_INIT='#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev
mount -t tmpfs tmp /tmp
echo "=== WLTIOS Boot ==="
echo "[wlt-init] seL4 -> Linux -> WLT-Init-Stub"
echo "[wlt-init] Starting /bin/sh (busybox 64-bit)..."
# Run init scripts (S98pscheck, S99tor, S99zpscheck)
for s in /etc/init.d/S*; do
  [ -x "$s" ] && "$s" start &
done
# Drop to shell on console
exec setsid /bin/sh < /dev/console > /dev/console 2>&1
'

build_rootfs() {
  local OUT=$1
  local WITH_TOR=$2
  cp "$BASE" "$OUT"
  rm -rf "$APPDIR"; mkdir -p "$APPDIR"
  
  # Replace init (cpio -A: last occurrence wins)
  mkdir -p "$APPDIR"
  printf '%s' "$FIXED_INIT" > "$APPDIR/init"
  chmod +x "$APPDIR/init"
  
  # ps-check script
  mkdir -p "$APPDIR/etc/init.d"
  if [ "$WITH_TOR" = "1" ]; then
    # vm1: S99tor + S99zpscheck
    cat > "$APPDIR/etc/init.d/S99tor" <<'TORINIT'
#!/bin/sh
case "$1" in
  start)
    echo "[tor-pd] Starting Tor daemon in vm1..."
    mkdir -p /var/lib/tor /var/run /tmp
    /usr/bin/tor -f /etc/tor/torrc > /tmp/tor.log 2>&1 &
    echo $! > /var/run/tor.pid
    echo "[tor-pd] Tor started (pid=$(cat /var/run/tor.pid 2>/dev/null))"
    ;;
esac
exit 0
TORINIT
    chmod +x "$APPDIR/etc/init.d/S99tor"
    
    # Copy Tor binary + deps
    mkdir -p "$APPDIR/usr/bin" "$APPDIR/etc/tor" "$APPDIR/usr/share/tor" "$APPDIR/var/lib/tor"
    cp "$TOR_SRC/usr/bin/tor" "$APPDIR/usr/bin/tor"
    cp "$TOR_SRC/etc/tor/torrc" "$APPDIR/etc/tor/torrc" 2>/dev/null || true
    cp -r "$TOR_SRC/usr/share/tor/"* "$APPDIR/usr/share/tor/" 2>/dev/null || true
    for lib in libz.so.1 libm.so.6 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 libsystemd.so.0 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.so.6 ld-linux-x86-64.so.2; do
      for f in $(find "$TOR_SRC/lib" "$TOR_SRC/lib64" -name "$lib*" 2>/dev/null); do
        rel="${f#$TOR_SRC/}"
        mkdir -p "$APPDIR/$(dirname "$rel")"
        cp -P "$f" "$APPDIR/$rel" 2>/dev/null || true
      done
    done
    
    cat > "$APPDIR/etc/init.d/S99zpscheck" <<'PS1'
#!/bin/sh
case "$1" in
  start)
    sleep 3
    echo "=====VM1-PS-BEGIN====="
    ps aux 2>/dev/null || ps
    echo "=====VM1-PS-END====="
    echo "[vm1-check] tor present: $(ps 2>/dev/null | grep -c '[t]or')"
    echo "[vm1-check] tor log:"
    tail -5 /tmp/tor.log 2>/dev/null || echo "(no tor log)"
    ;;
esac
exit 0
PS1
    chmod +x "$APPDIR/etc/init.d/S99zpscheck"
  else
    # vm0: S98pscheck only
    cat > "$APPDIR/etc/init.d/S98pscheck" <<'PS0'
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
PS0
    chmod +x "$APPDIR/etc/init.d/S98pscheck"
  fi
  
  ( cd "$APPDIR" && find . -print0 | cpio -o0A -H newc -O "$OUT" 2>/dev/null )
  echo "Built $OUT ($(stat -c%s "$OUT") bytes)"
}

echo "=== Building vm0 rootfs (64-bit, NO Tor) ==="
build_rootfs "$VM0_OUT" 0

echo ""
echo "=== Building vm1 rootfs (64-bit, HAS Tor) ==="
build_rootfs "$VM1_OUT" 1

echo ""
echo "=== Verify ==="
echo "--- vm0 ---"
cpio -itv < "$VM0_OUT" 2>/dev/null | grep -E "S98pscheck|usr/bin/tor$|^.*init$" | head -5
echo "--- vm1 ---"
cpio -itv < "$VM1_OUT" 2>/dev/null | grep -E "S99tor|S99zpscheck|usr/bin/tor$|^.*init$" | head -5
echo "[DONE]"
