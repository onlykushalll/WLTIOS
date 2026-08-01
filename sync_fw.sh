#!/bin/bash
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BUILD=/home/kushal/wltios-build
cp -r "$REPO/sel4-build/projects/vm/components/FirewallPD" "$BUILD/projects/vm/components/"
cp -r "$REPO/sel4-build/projects/vm-examples/apps/x86/firewall_pd_64" "$BUILD/projects/vm-examples/apps/x86/"
echo "SYNCED"
ls -la "$BUILD/projects/vm/components/FirewallPD/"
ls -la "$BUILD/projects/vm-examples/apps/x86/firewall_pd_64/"
