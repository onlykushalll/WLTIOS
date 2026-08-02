# PHASE: Fix the UEFI Boot Bug + Lock In seL4 Reproducibility + Finish TorPD Verification

**Status:** Ready to execute
**Target:** ~8-10 hours
**Priority order matters here:** Section A first — it's the highest-severity
open finding (a boot path that can silently proceed after a failed
firmware handoff). Sections B and C can run in either order after that.

**How to use this doc:** work top to bottom within each section, check off
each subtask only when genuinely verified (not just written). Report once
at the end covering all sections, same discipline as recent sessions —
raw command/log output, not restated claims.

---

## Section A — Fix the UEFI handoff bug (do this first)

**Why this matters:** `boot.c`'s new handoff code (from `92fe3cc`) jumps to
`kernel.entry_point` without checking whether `ExitBootServices` actually
succeeded. If it fails — which UEFI firmware is specifically designed to do
when the memory-map key is stale — boot services remain active while the
kernel runs, which is undefined/catastrophic behavior for any real OS
kernel. This was never boot-tested against real or emulated firmware; only
compiled and linked.

### A.1 — Verify the struct offsets against a real reference
- [ ] Find or vendor real UEFI type definitions (`gnu-efi`'s `<efi.h>`, or
      the UEFI spec PDF) rather than trusting hand-recalled byte offsets
- [ ] Confirm or correct: `BootServices` at offset 96 in `EFI_SYSTEM_TABLE`
      (this one checked out on manual recount, re-verify against the real
      header anyway), `GetMemoryMap` and `ExitBootServices` offsets within
      `EFI_BOOT_SERVICES` (these were NOT independently confirmed —
      possible mismatch, verify precisely, don't assume either way)
- [ ] Replace the raw `(char*)ptr + N` offset arithmetic with real typed
      struct access once the correct definitions are in hand — this
      removes the entire class of "did I get the magic number right" risk
      going forward, not just fixes today's instance

### A.2 — Fix the memory-map buffer handling
- [ ] Currently fixed at 16KB with no growth path. If `GetMemoryMap`
      returns "buffer too small," use the size it reports as actually
      required rather than retrying the same undersized buffer
- [ ] Consider allocating the buffer dynamically (via `AllocatePool`) sized
      to what the firmware reports, rather than a fixed stack buffer

### A.3 — Check ExitBootServices' actual return value before jumping
- [ ] After the retry loop, explicitly check whether `ExitBootServices`
      succeeded. If it didn't, do not jump to the kernel — log an error
      and halt/reset instead. A boot that fails loudly is far better than
      one that proceeds with firmware still active.

### A.4 — Real verification, not just another compile
- [ ] Build succeeding (`wlt-boot.efi` compiling/linking) does not test any
      of this — get this actually running under OVMF (QEMU's UEFI
      firmware) or equivalent emulated UEFI environment, not just
      compiled. If OVMF isn't available in this environment, say so
      precisely and document exactly what setup would be needed, rather
      than treating "it compiles" as sufficient evidence this works.
- [ ] If real UEFI boot-testing genuinely isn't achievable in this
      environment, that's a legitimate, honestly-reported limitation —
      but the offset-verification (A.1) and the missing-status-check fix
      (A.3) should still happen regardless, since those are verifiable by
      code inspection alone.

---

## Section B — Lock in seL4 component reproducibility

**Why this matters:** Firewall PD's real source files (verified genuine
and honestly scoped) exist only in the gitignored `sel4-build/` tree — the
tracked repo only has the shell script that generates them. This is the
same gap TorPD's very first scaffold attempt had, early in this project.
Delete `sel4-build/` today and Firewall PD doesn't come back without
manually re-running a script.

### B.1 — Firewall PD
- [ ] Commit the actual generated files (`FirewallPD.camkes`,
      `firewall_pd.c`, `minimal_firewall.camkes`, `CMakeLists.txt`) into
      the tracked repo — not just the generator script. Check where
      TorPD's equivalent files eventually landed (if they did) and follow
      the same convention for consistency.
- [ ] Confirm the generator script (`create_firewall_pd.sh`) and the
      committed files actually match — if they've drifted since the
      script last ran, reconcile them rather than committing whichever is
      more convenient.

### B.2 — Audit for the same gap elsewhere
- [ ] Check whether TorPD's current (per-VM rootfs, 64-bit) component
      files have the same tracked-vs-gitignored gap, given how much of
      that work happened directly in `sel4-build/`/the WSL build tree
      this round. If so, fix it the same way.
- [ ] General check: is there anything else this project has called
      "done" that only exists in a gitignored or build-only location? A
      quick `git status`/`git check-ignore` sweep of recently-touched
      seL4 paths would catch this class of gap directly.

### B.3 — Real rebuild test
- [ ] With B.1/B.2 fixed: delete the relevant portion of the local build
      tree and confirm Firewall PD's scaffold + IPC proof rebuilds and
      boots identically from the tracked repo alone — the actual bar this
      project has held every other component to.

---

## Section C — Finish TorPD's actual verification

**Why this matters:** Per-VM rootfs isolation is now proven at the
*content* level (vm0's rootfs genuinely lacks Tor, vm1's genuinely has it,
different RAMDISK sizes prove different images load) — but the original
ask (a live process listing from inside each running VM, and the actual
VM-to-VM IPC ping/pong round-trip) is still not confirmed. The report
found that boot messages come from the VMM itself, not the guest rootfs —
a real, useful diagnostic, but it means the init/process-visibility
question is still open.

### C.1 — Get a real process listing from each running VM
- [ ] Now that per-VM rootfs isolation is proven, get an actual `ps`-
      equivalent listing from inside vm0 and vm1 during a boot test —
      confirming Tor's process is in vm1's list and absent from vm0's,
      not just that vm1's rootfs *contains* the Tor binary on disk
- [ ] The VMM-vs-rootfs boot-message confusion (the same "=== WLTIOS
      Boot ===" text appearing regardless of what's in `/init`) needs
      tracing to its actual source before this can work reliably — find
      where in the VMM this gets printed and confirm it's not masking or
      interfering with the guest's real console output

### C.2 — The VM-to-VM IPC ping/pong test (the original ADR-09 risk,
      still unconfirmed since the very start of this project)
- [ ] `vm_ipc_test` and its shared-memory device path issue were flagged
      as blocking this in an earlier round — check whether that's still
      blocking now that the rootfs/init issues from this round are
      better understood, or whether it's a separate, still-open problem
- [ ] Real serial log showing an actual ping sent from one VM and a pong
      received on the other. This is the single most-deferred proof in
      this entire project — if it's still not achievable this round, say
      exactly why in enough detail that the next attempt doesn't have to
      re-derive the same diagnosis from scratch.

### C.3 — Tor actually running inside vm1, confirmed live (not just
      present on disk)
- [ ] With C.1's process listing working: confirm Tor's daemon process
      is actually running (not just installed) inside vm1 during a real
      boot test, and ideally that it reaches at least partial bootstrap

---

## Standing rules

Reproducibility (this entire phase is largely *about* reproducibility —
hold it to that standard especially strictly this round). Real evidence:
raw serial logs, raw command output, actual file listings — not restated
summaries. Commit messages match diffs. Confirm anything claimed "done" is
reachable from the tracked repo alone, not just present in a build
directory. One batched report at the end covering all three sections.
