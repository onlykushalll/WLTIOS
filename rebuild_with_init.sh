#!/bin/bash
# C.1: Rebuild rootfs with proper /init that runs ps + starts Tor (vm1).
# The key fix: provide /init so the kernel doesn't fall back to busybox's
# built-in init (which prints the confusing "=== WLTIOS Boot ===" messages).
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-c1

echo "=== Extract base rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
echo "Files: $(find . -type f | wc -f), Symlinks: $(find . -type l | wc -l)"

# Remove old wlt-shell symlink + provide proper /init
rm -f bin/wlt-shell init

# --- vm0 /init: mounts + ps + drop to shell ---
cat > init << 'INIT0'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev
mount -t tmpfs tmp /tmp
echo "=====VM0-BOOT-INIT-RAN====="
echo "[vm0] Process listing:"
ps
echo "=====VM0-PS-DONE====="
echo "[vm0] tor present: $(ps | grep -c '[t]or')"
exec setsid /bin/sh < /dev/console > /dev/console 2>&1
INIT0
chmod +x init

# Build vm0
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"
echo "vm0 /init: $(file init)"

# --- vm1 /init: mounts + start Tor + ps + drop to shell ---
# Add Tor binary + deps
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true
for lib in libz.so.1 libm.so.6 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 libsystemd.so.0 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.so.6 ld-linux-x86-64.so.2; do
    for f in $(find "$TOR_SRC/lib" "$TOR_SRC/lib64" -name "$lib*" 2>/dev/null); do
        rel="${f#$TOR_SRC/}"
        mkdir -p "$(dirname "$rel")"
        cp -P "$f" "$rel" 2>/dev/null || true
    done
done

cat > init << 'INIT1'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev
mount -t tmpfs tmp /tmp
echo "=====VM1-BOOT-INIT-RAN====="
echo "[vm1] Starting Tor daemon..."
mkdir -p /var/lib/tor /var/run
/usr/bin/tor -f /etc/tor/torrc > /tmp/tor.log 2>&1 &
TORPID=$!
echo "[vm1] Tor PID: $TORPID"
sleep 5
echo "[vm1] Process listing:"
ps
echo "=====VM1-PS-DONE====="
echo "[vm1] tor present: $(ps | grep -c '[t]or')"
echo "[vm1] tor log:"
tail -5 /tmp/tor.log 2>/dev/null
echo "=====VM1-TOR-LOG-DONE====="
exec setsid /bin/sh < /dev/console > /dev/console 2>&1
INIT1
chmod +x init

find . -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"
echo "vm1 /init: $(file init)"

echo ""
echo "=== Verify ==="
echo "vm0 /init exists: $(cd /tmp && rm -rf v0 && mkdir v0 && cd v0 && cpio -idm < "$VM0_OUT" 2>/dev/null && ls init 2>&1)"
echo "vm0 /init content:"
(cd /tmp/v0 && head -3 init)
echo "vm1 /init exists: $(cd /tmp && rm -rf v1 && mkdir v1 && cd v1 && cpio -idm < "$VM1_OUT" 2>/dev/null && ls init 2>&1)"
echo "vm1 /init content:"
(cd /tmp/v1 && head -3 init)
echo "vm1 has tor: $(cd /tmp/v1 && file usr/bin/tor 2>/dev/null)"
echo "vm0 no tor: $(cd /tmp/v0 && ls usr/bin/tor 2>&1)"

echo ""
echo "=== Remove rdinit from cmdline (let kernel use /init naturally) ==="
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes
sed -i 's| rdinit=/bin/sh||' "$CAMKES"
grep VM_GUEST_CMDLINE "$CAMKES" | head -1
echo "[DONE]"
