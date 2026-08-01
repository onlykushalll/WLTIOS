#!/bin/bash
# A.4: Build per-VM rootfs with ps-check init scripts for boot-test evidence.
# vm0: uClibc rootfs (NO Tor) + S98pscheck -> prints "VM0-PS-BEGIN" + ps + "VM0-PS-END"
# vm1: musl rootfs (HAS Tor) + S99tor (with ps) + S98pscheck -> prints "VM1-PS-BEGIN" + ps + "VM1-PS-END"
set -e

VM0_SRC=/home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio
VM1_SRC=/home/kushal/wltios-build/rootfs-wlt-full.cpio
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor.cpio
APPDIR=/tmp/ps-overlay

# ---- S98pscheck for vm0 (no Tor) ----
echo "[A.4] Building vm0 rootfs (no Tor) with ps-check..."
cp "$VM0_SRC" "$VM0_OUT"
rm -rf "$APPDIR"; mkdir -p "$APPDIR/etc/init.d"
cat > "$APPDIR/etc/init.d/S98pscheck" <<'PSCHECK0'
#!/bin/sh
# WLTIOS A.4: print vm0's process list to serial for isolation proof.
# vm0 must NOT have tor in its process list.
case "$1" in
  start)
    sleep 3  # let other daemons settle
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
echo "[A.4] vm0 rootfs: $VM0_OUT ($(stat -c%s "$VM0_OUT") bytes)"

# ---- S99tor (with ps) + S98pscheck for vm1 (has Tor) ----
echo "[A.4] Building vm1 rootfs (has Tor) with ps-check..."
cp "$VM1_SRC" "$VM1_OUT"
rm -rf "$APPDIR"; mkdir -p "$APPDIR/etc/init.d"

# S98pscheck for vm1 - runs AFTER S99tor so Tor is in the list
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
# WLTIOS TorPD - Tor auto-start for vm1 only.
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

# S99pscheck - runs after S99tor (S99 sorts alphabetically, so use S99zpscheck to run last)
cat > "$APPDIR/etc/init.d/S99zpscheck" <<'PSCHECK1'
#!/bin/sh
# WLTIOS A.4: print vm1's process list to serial for isolation proof.
# vm1 MUST have tor in its process list.
case "$1" in
  start)
    sleep 3  # let Tor start
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
echo "[A.4] vm1 rootfs: $VM1_OUT ($(stat -c%s "$VM1_OUT") bytes)"

# Verify
echo "--- vm0 verify (no tor, has pscheck) ---"
cpio -itv < "$VM0_OUT" 2>/dev/null | grep -E "S98pscheck|usr/bin/tor" | head
echo "--- vm1 verify (has tor, has pscheck) ---"
cpio -itv < "$VM1_OUT" 2>/dev/null | grep -E "S99tor|S99zpscheck|usr/bin/tor$" | head
echo "[A.4] DONE - both rootfs built"
