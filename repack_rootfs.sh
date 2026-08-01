#!/bin/bash
# Extract rootfs, replace /bin/wlt-shell + add init scripts, repack.
# Uses cpio -idm (preserves symlinks) + cpio -o -H newc (repacks).
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-repack

echo "=== Extracting base rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"
cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
echo "Extracted. Files: $(find . -type f | wc -l), Symlinks: $(find . -type l | wc -l)"
echo "Original wlt-shell: $(file bin/wlt-shell)"

# Replace /bin/wlt-shell with a shell script
cat > bin/wlt-shell << 'WLTSH'
#!/bin/sh
# WLTIOS: wlt-shell shim - runs init scripts then drops to /bin/sh
# (original wlt-shell was 32-bit i386, incompatible with 64-bit seL4 guest kernel)
echo "[wlt-shell] Running init scripts..."
for s in /etc/init.d/S*; do
  [ -x "$s" ] && "$s" start &
done
sleep 10
echo "[wlt-shell] Dropping to shell."
exec /bin/sh
WLTSH
chmod +x bin/wlt-shell
echo "New wlt-shell: $(file bin/wlt-shell)"

# --- Build vm0 (no Tor) ---
echo ""
echo "=== Building vm0 (no Tor) ==="
mkdir -p etc/init.d
cat > etc/init.d/S98pscheck << 'PS0'
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
chmod +x etc/init.d/S98pscheck
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $VM0_OUT ($(stat -c%s "$VM0_OUT") bytes)"

# --- Build vm1 (has Tor) ---
echo ""
echo "=== Building vm1 (has Tor) ==="
# Add Tor binary + deps + torrc
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true
# Copy Tor shared lib deps
for lib in libz.so.1 libm.so.6 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 libsystemd.so.0 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.so.6 ld-linux-x86-64.so.2; do
  for f in $(find "$TOR_SRC/lib" "$TOR_SRC/lib64" -name "$lib*" 2>/dev/null); do
    rel="${f#$TOR_SRC/}"
    mkdir -p "$(dirname "$rel")"
    cp -P "$f" "$rel" 2>/dev/null || true
  done
done
# S99tor init script
cat > etc/init.d/S99tor << 'TORINIT'
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
chmod +x etc/init.d/S99tor
# S99zpscheck
cat > etc/init.d/S99zpscheck << 'PS1'
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
chmod +x etc/init.d/S99zpscheck

find . -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $VM1_OUT ($(stat -c%s "$VM1_OUT") bytes)"

# Verify
echo ""
echo "=== Verify ==="
echo "vm0 wlt-shell: $(cd /tmp && rm -rf v0 && mkdir v0 && cd v0 && cpio -idm < "$VM0_OUT" 2>/dev/null && file bin/wlt-shell)"
echo "vm1 has tor: $(cd /tmp && rm -rf v1 && mkdir v1 && cd v1 && cpio -idm < "$VM1_OUT" 2>/dev/null && ls -la usr/bin/tor)"
echo "vm0 no tor: $(cd /tmp/v0 && ls usr/bin/tor 2>&1)"
echo "Symlinks preserved: vm0=$(cd /tmp/v0 && find . -type l | wc -l), vm1=$(cd /tmp/v1 && find . -type l | wc -l)"
echo "[DONE]"
