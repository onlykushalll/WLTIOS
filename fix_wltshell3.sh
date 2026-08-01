#!/bin/bash
# Fix v3: /bin/wlt-shell runs init scripts (S98pscheck, S99tor, S99zpscheck) then drops to shell.
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
APPDIR=/tmp/wlt-fix3-overlay

# wlt-shell: runs init scripts, waits, drops to shell
WLT_SCRIPT='#!/bin/sh
# WLTIOS: wlt-shell shim - runs init scripts then drops to /bin/sh
# (original wlt-shell was 32-bit, incompatible with 64-bit kernel)
echo "[wlt-shell] Running init scripts..."
for s in /etc/init.d/S*; do
  [ -x "$s" ] && "$s" start &
done
# Wait for init scripts (ps-check needs sleep 5, tor needs time to start)
sleep 10
echo "[wlt-shell] Init scripts started. Dropping to shell."
exec /bin/sh
'

build_rootfs() {
  local OUT=$1
  local WITH_TOR=$2
  cp "$BASE" "$OUT"
  rm -rf "$APPDIR"; mkdir -p "$APPDIR/bin" "$APPDIR/etc/init.d"
  printf '%s\n' "$WLT_SCRIPT" > "$APPDIR/bin/wlt-shell"
  chmod +x "$APPDIR/bin/wlt-shell"
  
  if [ "$WITH_TOR" = "1" ]; then
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
    cat > "$APPDIR/etc/init.d/S99zpscheck" <<'PS1'
#!/bin/sh
case "$1" in
  start)
    sleep 6
    echo "=====VM1-PS-BEGIN====="
    ps aux 2>/dev/null || ps
    echo "=====VM1-PS-END====="
    echo "[vm1-check] tor present: $(ps 2>/dev/null | grep -c '[t]or')"
    echo "[vm1-check] tor log:"
    tail -10 /tmp/tor.log 2>/dev/null || echo "(no tor log)"
    ;;
esac
exit 0
PS1
    chmod +x "$APPDIR/etc/init.d/S99zpscheck"
  else
    cat > "$APPDIR/etc/init.d/S98pscheck" <<'PS0'
#!/bin/sh
case "$1" in
  start)
    sleep 6
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

echo "=== Building vm0 (no Tor) ==="
build_rootfs "$VM0_OUT" 0
echo "=== Building vm1 (has Tor) ==="
build_rootfs "$VM1_OUT" 1
echo "[DONE]"
