#!/bin/bash
# Build rootfs with marker /init + remove rdinit from cmdline (let kernel use /init).
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-marker
INIT_SRC=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/marker_init.sh

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# Replace /init with marker script
cp "$INIT_SRC" init
dos2unix init 2>/dev/null
chmod +x init
echo "New /init: $(file init)"

# Replace 32-bit binaries with stubs
rm -f usr/sbin/consumes_event_init usr/sbin/emits_event_init
echo '#!/bin/sh' > usr/sbin/consumes_event_init
echo '#!/bin/sh' > usr/sbin/emits_event_init
chmod +x usr/sbin/consumes_event_init usr/sbin/emits_event_init
# Replace wlt-shell with symlink to busybox
rm -f bin/wlt-shell
ln -s busybox bin/wlt-shell

# vm0
cat > etc/init.d/S98pscheck << 'PS0'
#!/bin/sh
case "$1" in
  start)
    sleep 5
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
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1 (add Tor)
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
cat > etc/init.d/S99zpscheck << 'PS1'
#!/bin/sh
case "$1" in
  start)
    sleep 8
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
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

# Also remove rdinit from cmdline (let kernel use /init naturally)
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes
sed -i 's| rdinit=/bin/sh||' "$CAMKES"
echo "Cmdline after fix:"
grep VM_GUEST_CMDLINE "$CAMKES" | head -1
echo "[DONE]"
