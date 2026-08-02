#!/bin/bash
# C.1-fix: Put /bin/wlt-shell back as a shell script.
# The custom wlt-init applet (in busybox) is PID 1 and tries to execve
# /bin/wlt-shell. If wlt-shell is a shell script, the kernel execs /bin/sh
# to interpret it. Since /bin/sh is NOT PID 1 (wlt-init is), busybox runs
# as a normal shell, not its init applet.
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-wltshell-fix

echo "=== Extract base rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# Keep the original /init (shell script that calls wlt-shell)
# The original /init calls: setsid /bin/wlt-shell < /dev/console > /dev/console 2>&1
# The wlt-init applet in busybox also calls /bin/wlt-shell.
# Either way, /bin/wlt-shell gets exec'd. Make it a useful script.

# Replace /bin/wlt-shell with a script that prints ps + drops to shell
cat > bin/wlt-shell << 'WLTSH'
#!/bin/sh
# WLTIOS wlt-shell replacement: prints process listing + drops to /bin/sh.
# This runs when the wlt-init applet (PID 1) execve's /bin/wlt-shell.
mount -t proc proc /proc 2>/dev/null
mount -t sysfs sys /sys 2>/dev/null
mount -t devtmpfs dev /dev 2>/dev/null
mount -t tmpfs tmp /tmp 2>/dev/null

echo "=====WLTIOS-PS-BEGIN====="
ps aux 2>/dev/null || ps
echo "=====WLTIOS-PS-END====="
echo "[wlt-check] tor present: $(ps 2>/dev/null | grep -c '[t]or')"

# Print tor log if exists
if [ -f /tmp/tor.log ]; then
    echo "=====WLTIOS-TOR-LOG====="
    tail -10 /tmp/tor.log 2>/dev/null
    echo "=====WLTIOS-TOR-LOG-END====="
fi

echo "[wlt-shell] Dropping to /bin/sh"
exec /bin/sh
WLTSH
chmod +x bin/wlt-shell
echo "wlt-shell: $(file bin/wlt-shell)"

# Restore /sbin/init (busybox init) so the kernel can run it
# (we removed it earlier; need it back so the kernel finds an init)
# Actually, /init is a shell script that calls wlt-shell, so /sbin/init
# is the fallback. Let's keep /sbin/init removed and let /init handle it.
# But /init IS a shell script, and busybox intercepts PID 1...
# 
# The key: keep /init as the ORIGINAL shell script (calls wlt-shell).
# The kernel runs /init -> busybox sh -> runs the script -> calls wlt-shell.
# But busybox sh as PID 1 runs init applet instead...
#
# Actually, the ORIGINAL /init already calls wlt-shell via setsid.
# And the wlt-init applet in busybox ALSO calls wlt-shell.
# Either path leads to /bin/wlt-shell being exec'd.
# So just make sure /bin/wlt-shell is useful.

# Restore original /init (the base rootfs has it)
# It's already there from the cpio extraction.

# --- vm0 (no Tor) ---
echo "=== Building vm0 ==="
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# --- vm1 (has Tor + S99tor) ---
echo "=== Building vm1 ==="
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

# S99tor init script (runs before wlt-shell via /etc/init.d/rcS)
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

echo ""
echo "=== Verify ==="
echo "vm0 wlt-shell: $(cd /tmp && rm -rf v0 && mkdir v0 && cd v0 && cpio -idm < "$VM0_OUT" 2>/dev/null && file bin/wlt-shell)"
echo "vm0 /init: $(cd /tmp/v0 && file init)"
echo "vm1 wlt-shell: $(cd /tmp && rm -rf v1 && mkdir v1 && cd v1 && cpio -idm < "$VM1_OUT" 2>/dev/null && file bin/wlt-shell)"
echo "vm1 has tor: $(cd /tmp/v1 && file usr/bin/tor 2>/dev/null)"
echo "vm0 no tor: $(cd /tmp/v0 && ls usr/bin/tor 2>&1)"

echo ""
echo "=== Restore rdinit=/init in cmdline ==="
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes
sed -i 's|init=/init|rdinit=/init|' "$CAMKES"
grep VM_GUEST_CMDLINE "$CAMKES" | head -1

echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -5

echo ""
echo "=== Boot test ==="
LOG=/home/kushal/torpd-c1-wltshell-fix.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== Check results ==="
strings "$LOG" | grep "WLTIOS-PS\|wlt-check\|tor present\|tor-pd\|Starting Tor\|WLTIOS-TOR-LOG" | head -15
echo "--- PS listing ---"
sed -n '/WLTIOS-PS-BEGIN/,/WLTIOS-PS-END/p' "$LOG" | strings | head -20
echo "--- Tor log ---"
sed -n '/WLTIOS-TOR-LOG/,/WLTIOS-TOR-LOG-END/p' "$LOG" | strings | head -10
