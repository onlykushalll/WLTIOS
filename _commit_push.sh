#!/bin/bash
cd /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS

# Clean up ALL temp scripts created during this session
rm -f _stats.ps1 _status.ps1 _cleanup.ps1 _dump.ps1 _run_tests.sh _run_tests2.sh \
      _gen_tests.sh _run_all.sh _run_final.sh _run_stubs.sh _check.sh _check_errors.sh \
      _check_types.sh _read_errors.sh _debug.sh _dump_api.sh _headers_dump.txt \
      _commit_and_push.sh 2>/dev/null

# Verify _test.txt is gone
if [ -f src/userspace/wlt-widgets/_test.txt ]; then
    rm -f src/userspace/wlt-widgets/_test.txt
    echo "Removed _test.txt"
fi

# Stage all changes
git add -A

# Show what will be committed
echo "=== Files to be committed ==="
git status --short | head -30
echo ""

# Commit
git commit -m "[test+chore+docs] 11 module tests + wlt-render Makefile + spec fixes

Task 1: Add wlt-render/Makefile (follows wlt-compositor pattern), remove
  wlt-widgets/_test.txt junk artifact.

Task 2: Add unit tests for 11 Day-2 modules â compositor, shell, pkg, net,
  crypto, sandbox, permissions, settings, tor_mgr, update, filemanager.
  11/11 suites pass (62 total assertions). 6 compile against real source,
  5 use stubs for external-dep functions (wlroots, network, crypto libs).
  Tests cover: error_str, init NULL safety, shutdown NULL safety, lifecycle,
  error paths. Follows tests/userspace/service_test.c pattern.

Task 3: Add src/userspace/wlt-init/wlt-init-stub.c â minimal init wrapper
  that redirects stdin/stdout/stderr to /dev/console before exec-ing
  /bin/wlt-shell, with retry loop (5 attempts) and /bin/sh fallback.
  Compiled static for Buildroot rootfs injection. Unblocks QEMU interactive
  testing (WLT-Shell was exiting immediately because stdin wasn't connected
  to serial console).

Task 4: Fix docs/specs/boot-verifier-pd.md â removed files.c from File
  Structure (a Microkit PD has no filesystem access). Added note that
  critical file hashes are pre-computed at build time and embedded as a
  constant array in hashes.h. Updated What's Verified table: runtime
  re-verification is memory-only (in-memory binaries), not config files.

Task 5: Add Key Persistence Architecture section to docs/specs/key-vault-pd.md.
  Resolves the circular dependency (need Key Vault to decrypt disk, need disk
  mounted before Key Vault's Linux-side client can run) using TPM-sealed
  bootstrap key: WLT-Boot unseals at UEFI time, passes to seL4, seL4 maps
  into Key Vault PD memory before Linux starts."

# Push
echo ""
echo "=== Pushing to GitHub ==="
git push origin main 2>&1 | tail -5

echo ""
echo "=== Final git log ==="
git log --oneline -5
echo ""
echo "=== Total commits ==="
git rev-list --count HEAD
