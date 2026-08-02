#!/bin/bash
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-no-redirect

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# wlt-shell: NO redirection — use inherited stdout from wlt-init.
# The wlt-init applet's messages DO appear in serial log, so its stdout
# IS connected to the serial console. wlt-shell inherits these FDs.
cat > bin/wlt-shell << 'WLTSH'
#!/bin/sh
mount -t proc proc /proc 2>/dev/null
mount -t sysfs sys /sys 2>/dev/null
mount -t devtmpfs dev /dev 2>/dev/null
mount -t tmpfs tmp /tmp 2>/dev/null

echo "=====WLTIOS-PS-BEGIN====="
ps aux 2>/dev/null || ps
echo "=====WLTIOS-PS-END====="
echo "[wlt-check] tor present: $(ps 2>/dev/null | grep -c '[t]or')"

if [ -f /tmp/tor.log ]; then
    echo "=====WLTIOS-TOR-LOG====="
    tail -10 /tmp/tor.log 2>/dev/null
    echo "=====WLTIOS-TOR-LOG-END====="
fi

echo "[wlt-shell] Dropping to /bin/sh"
exec /bin/sh
WLTSH
chmod +x bin/wlt-shell

# vm0
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
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
find . -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo "=== Rebuild + boot ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

LOG=/home/kushal/torpd-c1-no-redirect.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== RESULTS ==="
echo "--- PS listing ---"
sed -n '/WLTIOS-PS-BEGIN/,/WLTIOS-PS-END/p' "$LOG" | strings | head -25
echo "--- wlt-check ---"
strings "$LOG" | grep "wlt-check\|tor present\|tor-pd\|Starting Tor" | head -10
echo "--- tor log ---"
sed -n '/WLTIOS-TOR-LOG/,/WLTIOS-TOR-LOG-END/p' "$LOG" | strings | head -10
echo "--- last 10 ---"
strings "$LOG" | tail -10
