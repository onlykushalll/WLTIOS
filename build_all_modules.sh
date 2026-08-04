#!/bin/bash
# Section A: Compile-verify all 8 fixes across 4 modules.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace

echo "================================================================"
echo "A.1 — wlt-net: net.c + wifi.c"
echo "================================================================"
cd "$REPO/wlt-net"
echo "--- make clean ---"
make clean 2>&1 | tail -3
echo "--- make ---"
make 2>&1
NET_EXIT=$?
echo "wlt-net exit: $NET_EXIT"

echo ""
echo "================================================================"
echo "A.2 — wlt-update: rollback.c"
echo "================================================================"
cd "$REPO/wlt-update"
echo "--- make clean ---"
make clean 2>&1 | tail -3
echo "--- make ---"
make 2>&1
UPD_EXIT=$?
echo "wlt-update exit: $UPD_EXIT"

echo ""
echo "================================================================"
echo "A.3 — wlt-defender: anomaly_engine.c + ebpf_engine.c + forensic_log.c + net_engine.c"
echo "================================================================"
cd "$REPO/wlt-defender"
echo "--- make clean ---"
make clean 2>&1 | tail -3
echo "--- make ---"
make 2>&1
DEF_EXIT=$?
echo "wlt-defender exit: $DEF_EXIT"

echo ""
echo "================================================================"
echo "A.4 — wlt-settings: network_panel.c"
echo "================================================================"
cd "$REPO/wlt-settings"
echo "--- make clean ---"
make clean 2>&1 | tail -3
echo "--- make ---"
make 2>&1
SET_EXIT=$?
echo "wlt-settings exit: $SET_EXIT"

echo ""
echo "================================================================"
echo "SUMMARY"
echo "================================================================"
echo "wlt-net:      $NET_EXIT"
echo "wlt-update:   $UPD_EXIT"
echo "wlt-defender: $DEF_EXIT"
echo "wlt-settings: $SET_EXIT"
