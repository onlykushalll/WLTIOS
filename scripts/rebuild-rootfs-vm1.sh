#!/usr/bin/env bash
# Rebuild rootfs_vm1_tor64.cpio with the WLTIOS overlay applied.
# The base cpio (tor 0.4.9.11 rootfs, Aug 9) has no repo build recipe --
# this script at least makes the WLTIOS deltas reproducible:
#   1. eth0 static config in /init (base /init never ran the init.d
#      scripts, so eth0 had NO address and Tor could not bootstrap)
#   2. /etc/network/interfaces eth0 stanza (inert with /init, but correct
#      for any future ifup-based path)
# Ownership is forced to root:root -- Tor hard-fails its DataDirectory
# ownership check otherwise (uid 1000 repack bug, caught in boot17).
set -e
BASE="${1:?usage: rebuild-rootfs-vm1.sh <base.cpio> <out.cpio> <overlay-dir>}"
OUT="${2:?}"
OVERLAY="${3:?}"
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
mkdir -p "$W/rfs"
(cd "$W/rfs" && zcat -f "$BASE" 2>/dev/null || true | cpio -idm --no-absolute-filenames) < "$BASE" 2>/dev/null || \
  (cd "$W/rfs" && cpio -idm --no-absolute-filenames < "$BASE")
cp -a "$OVERLAY"/. "$W/rfs"/
(cd "$W/rfs" && find . | cpio -o -H newc --owner root:root) > "$OUT"
echo "[rebuild-rootfs-vm1] wrote $OUT ($(stat -c %s "$OUT") bytes)"
