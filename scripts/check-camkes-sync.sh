#!/bin/bash
# WLTIOS camkes drift check -- gap #26 recurrence guard.
# The tracked camkes app (src/sel4-partitions/tor-pd/camkes/app/) must be
# byte-identical to the live seL4 build-tree copy that actually boots.
# WHAT BOOTS MUST BE WHAT GIT TRACKS.
#
# Exit 0 = in sync (or build tree absent: fresh clone / CI).
# Exit 1 = DRIFT. Reconcile by copying the build-tree file into the repo,
#          reviewing, and committing -- or delete the build-tree copy so the
#          next apply-patches.sh bootstraps it from git.
#
# Usage: bash scripts/check-camkes-sync.sh   (wired into pre-commit-check.sh
#        and packages/sel4-patches/apply-patches.sh)

REPO="$(cd "$(dirname "$BASH_SOURCE[0]}")/.." && pwd)"
BT="${WLTIOS_BUILD_TREE:-/home/kushal/wltios-build}"
LIVE_APP="$BT/projects/vm-examples/apps/x86/tor_pd_vm1"
APP="$REPO/src/sel4-partitions/tor-pd/camkes/app"

if [ ! -d "$LIVE_APP" ]; then
    echo "[camkes-sync] build tree not found at $LIVE_APP -- skipping (fresh clone?)"
    exit 0
fi

FAIL=0
for f in minimal_tor_pd.camkes src/cross_vm_connections_vm1.c src/cross_vm_connections.c app_settings.cmake CMakeLists.txt; do
    if [ ! -f "$APP/$f" ]; then
        echo "[camkes-sync] MISSING in repo: $f"
        FAIL=1
    elif [ ! -f "$LIVE_APP/$f" ]; then
        echo "[camkes-sync] MISSING in build tree: $f (copy from repo or delete to re-bootstrap)"
        FAIL=1
    elif ! cmp -s "$APP/$f" "$LIVE_APP/$f"; then
        echo "[camkes-sync] DRIFT: $f"
        echo "    repo      $(md5sum "$APP/$f" | cut -d' ' -f1)"
        echo "    build tree $(md5sum "$LIVE_APP/$f" | cut -d' ' -f1)"
        FAIL=1
    fi
done

if [ $FAIL -eq 0 ]; then
    echo "[camkes-sync] OK: tracked camkes app == build tree (tor_pd_vm1)"
else
    echo "[camkes-sync] DRIFT DETECTED -- what boots is NOT what git tracks" >&2
fi
exit $FAIL
