#!/bin/bash
# Find the build system for each affected module.
echo "=== 1. Find Makefiles/CMakeLists for wlt-net, wlt-update, wlt-defender, wlt-settings ==="
find /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src -name "Makefile" -o -name "CMakeLists.txt" 2>/dev/null | grep -E "net|update|defender|settings" | head -20

echo ""
echo "=== 2. Find the specific source files ==="
find /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src -name "net.c" -o -name "wifi.c" -o -name "rollback.c" -o -name "anomaly_engine.c" -o -name "ebpf_engine.c" -o -name "forensic_log.c" -o -name "net_engine.c" -o -name "network_panel.c" 2>/dev/null | head -20

echo ""
echo "=== 3. Check wlt-net structure ==="
ls -la /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace/wlt-net/ 2>/dev/null | head -15

echo ""
echo "=== 4. Check wlt-defender structure ==="
ls -la /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace/wlt-defender/ 2>/dev/null | head -15

echo ""
echo "=== 5. Check wlt-update structure ==="
ls -la /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace/wlt-update/ 2>/dev/null | head -15

echo ""
echo "=== 6. Check wlt-settings structure ==="
ls -la /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace/wlt-settings/ 2>/dev/null | head -15
