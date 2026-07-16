#!/bin/sh
set -e

# Work in a clean directory
mkdir -p /home/kushal/tmp-repack
cd /home/kushal/tmp-repack
rm -rf rootfs

# Create our S90 script
cat << 'EOF' > S90camkes_init
#!/bin/sh
echo "================================================="
echo "Key Vault Test Script Starting..."
echo "================================================="

MAX_TRIES=10
TRIES=0
while [ ! -c /dev/uio0 ] || [ ! -c /dev/uio1 ]; do
    if [ $TRIES -ge $MAX_TRIES ]; then
        echo "Timeout waiting for /dev/uio* devices"
        exit 1
    fi
    echo "Waiting for uio devices... (Try $TRIES/$MAX_TRIES)"
    sleep 1
    TRIES=$((TRIES + 1))
done

echo "UIO devices found! Interrupt mapping in /proc/interrupts:"
cat /proc/interrupts

echo "Starting consumes_event_wait on uio1 in background..."
consumes_event_wait /dev/uio1 &
WAIT_PID=$!

# Give it a moment to open the device and block on read
sleep 1

echo -n "ping" | dataport_write /dev/uio0 4096
echo "Emitting ready event on uio0..."
emits_event_emit /dev/uio0

# Sleep to allow VMM to callback and inject IRQ
sleep 2

echo "After 2 seconds, /proc/interrupts:"
cat /proc/interrupts

echo "Checking if background wait finished..."
if ps | grep -v grep | grep -q "$WAIT_PID"; then
    echo "Wait process $WAIT_PID still running. Wait failed."
    kill $WAIT_PID
else
    echo "Wait process finished! Reading result..."
    A_read_result=$(dataport_read /dev/uio1 4096)
    echo "Result from Key Vault: $A_read_result"
    case "$A_read_result" in
        *pong*)
            echo "TEST PASSED"
            ;;
        *)
            echo "TEST FAILED"
            ;;
    esac
fi
echo "================================================="
echo "Key Vault Test Finished!"
echo "================================================="
EOF
chmod +x S90camkes_init

# Extract, inject, repack under fakeroot
fakeroot bash -c "
  mkdir rootfs && cd rootfs
  # Extract
  cpio -id < /home/kushal/wltios-build/projects/vm-linux/images/rootfs/32/default_buildroot_rootfs-bare.cpio.ORIG
  # Inject
  cp ../S90camkes_init etc/init.d/S90camkes_init
  cp /home/kushal/wltios-build/build-keyvault/camkes-linux-artifacts/connector-modules/connection.ko lib/modules/4.8.16/kernel/drivers/vmm/connection.ko
  cp /home/kushal/wltios-build/build-keyvault/camkes-linux-artifacts/connector-apps/pkgs/emits_event/emits_event_emit usr/sbin/emits_event_emit
  cp /home/kushal/wltios-build/build-keyvault/camkes-linux-artifacts/connector-apps/pkgs/consumes_event/consumes_event_wait usr/sbin/consumes_event_wait
  cp /home/kushal/wltios-build/build-keyvault/camkes-linux-artifacts/connector-apps/pkgs/dataport/dataport_read usr/sbin/dataport_read
  cp /home/kushal/wltios-build/build-keyvault/camkes-linux-artifacts/connector-apps/pkgs/dataport/dataport_write usr/sbin/dataport_write
  cp /home/kushal/wltios-build/build-keyvault/camkes-linux-artifacts/connector-apps/pkgs/string_reverse/string_reverse usr/sbin/string_reverse
  # Repack
  find . -print0 | cpio --null -o -H newc > /home/kushal/wltios-build/projects/vm-linux/images/rootfs/32/default_buildroot_rootfs-bare.cpio
"

echo "Repack successful!"
