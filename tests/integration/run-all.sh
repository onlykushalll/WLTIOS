#!/bin/bash
# WLTIOS Integration Test Suite
# Boots QEMU, sends commands via serial, verifies output
# Usage: ./tests/integration/run-all.sh

set +e
KERNEL=~/wltios-build/debian-kernel-i386
INITRAMFS=~/wltios-build/wltios-initramfs.cpio.gz
LOG=/tmp/wlt-integration.log
PASS=0
FAIL=0

echo "=== WLTIOS Integration Test Suite ==="
echo ""

# Boot QEMU and pipe commands
(sleep 50
echo "netcheck"
sleep 8
echo "killswitch on"
sleep 5
echo "tcptest 10.0.2.2 80"
sleep 8
echo "torcheck"
sleep 10
echo "killswitch off"
sleep 3
echo "cat /etc/wltios-release"
sleep 2
echo "draw"
sleep 8
echo "sandbox /sbin/wlt-net"
sleep 5
echo "cat /proc/meminfo"
sleep 3
echo exit
) | timeout 110 qemu-system-i386 \
    -machine pc -cpu host -m 512M -enable-kvm \
    -kernel $KERNEL -initrd $INITRAMFS \
    -netdev user,id=net0 -device e1000,netdev=net0 \
    -vga std -display none \
    -serial mon:stdio \
    -append "console=ttyS0 init=/sbin/init vga=792" \
    > $LOG 2>&1

# Verify each test
check() {
    local name=$1
    local pattern=$2
    if grep -qa "$pattern" $LOG; then
        echo "  PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $name (expected: $pattern)"
        FAIL=$((FAIL + 1))
    fi
}

echo "--- Test Results ---"
check "Network up" "10.0.2.15: FOUND"
check "Kill switch on" "OUTPUT policy: DROP"
check "TCP blocked" "BLOCKED"
check "Tor SOCKS" "Connected to Tor SOCKS5"
check "Kill switch off" "OUTPUT policy: ACCEPT"
check "OS version" "WLTIOS_VERSION=0.1.0"
check "Draw" "Background filled"
check "Sandbox isolation" "No such device"
check "Framebuffer" "Framebuffer: 1024x768"
check "Amnesic mode" "Amnesic mode"

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
exit $FAIL