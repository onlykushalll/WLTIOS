# PHASE: seL4 Completion + Verified Boot Crypto + Desktop/UI Polish

**Status:** Ready to execute
**Target:** ~5-6 hours
**Prereqs:** none — all four workstreams below are independent of each
other except where explicitly noted (Workstream 1 and 3 both touch the
seL4 build tree — do not run them concurrently in separate sessions;
sequence 1 before 3, or confirm no overlapping build artifacts first)

**How to use this doc:** work top to bottom within each workstream.
Check off each subtask as it's genuinely verified (compiled, tested,
real evidence captured) — not when the code is merely written. Report
once at the end covering all four workstreams, same self-verification
discipline as the last several sessions (grep/wc-l/git show checks
pasted as raw output, not bare "verified" claims).

---

## Workstream 1 — TorPD: find vm1 (carries over, still unresolved)

**Why this matters:** this is the actual point of TorPD. vm0 boots to
Buildroot login; vm1 has never shown a single line of output. Until
this is resolved, TorPD is an elaborate two-VM scaffold running nothing
in the second VM.

### 1.1 — Diagnose the silence
- [ ] Get the raw, full serial log from an actual current boot attempt
      (not a grep summary — read the whole thing)
- [ ] Confirm whether vm1's VMM process even starts (check for any
      vm1-prefixed log lines, init messages, or crash/fault output
      anywhere in the log, not just a "vm1" substring search)
- [ ] Check the QEMU/boot script's serial console wiring — in a
      two-VM CAmkES setup, each VM typically needs its own console
      device mapped to a distinguishable output (a second `-serial`
      target, a second virtual UART, or a multiplexed console); confirm
      whether vm0's console is the only one actually connected to
      anything observable
- [ ] Check camkes-vm-examples (or equivalent upstream multi-VM
      reference project, same source used for the heap_size fix
      research) specifically for how per-VM console/serial output is
      configured — don't guess at a fix before checking

### 1.2 — Fix and confirm vm1 boots
- [ ] Apply whatever the research in 1.1 reveals
- [ ] Real boot test: capture and paste the raw serial log showing
      vm1 reaching its own userspace (a login prompt, a shell, or
      equivalent — something unambiguous)
- [ ] If vm1 genuinely cannot be made to boot this session, stop here
      and document precisely what's blocking it (exact error, exact
      log excerpt) — do not spend the rest of the session's time
      budget stuck on this one subtask; move to 1.3 only if 1.2 is
      resolved, otherwise move on to Workstream 2

### 1.3 — VM-to-VM IPC (the actual unproven risk from ADR-09)
- [ ] Once vm1 boots: attempt the real IPC round-trip between vm0 and
      vm1 (ping/pong minimum, same bar Key Vault PD was held to)
- [ ] Raw serial log proving both directions

### 1.4 — Tor daemon inside vm1
- [ ] Get Tor's actual userspace daemon running inside vm1 (reuse the
      already-vendored Tor binary + libs from packages/tor-libs/ —
      don't re-solve library vendoring from scratch)
- [ ] Confirm via serial log that Tor starts inside vm1, not vm0

**If 1.1-1.2 consume the full time budget for this workstream, that's
a legitimate outcome — report exactly what was found and what's still
blocking, don't compress into a false pass.**

---

## Workstream 2 — Verified Boot: real Ed25519 (carries over, still a stub)

**Why this matters:** `wlt_verify_ed25519` is fail-closed now (correct)
but has no real cryptography behind it — it refuses everything because
nothing is implemented, not because it's actually checking a signature.

### 2.1 — Vendor monocypher properly
- [ ] Fetch monocypher (single-file C, no dependencies) and commit it
      into the repo under `packages/` (or wherever this project's
      vendoring convention places crypto deps — check `packages/
      sodium-libs/` and `packages/tor-libs/` for the established
      pattern and match it)
- [ ] This must be a real, committed, reproducible addition — not a
      scratch-only fix that vanishes next session (the curl and
      libsodium sagas earlier in this project are the cautionary
      precedent; don't repeat that mistake a third time)
- [ ] Confirm it builds standalone before wiring it into verify.c

### 2.2 — Real Ed25519 verification
- [ ] Replace the stub body of `wlt_verify_ed25519` with a real call
      into monocypher's Ed25519 verify function
- [ ] Wire `sign-boot.sh`'s existing key-generation to actually
      produce a keypair this code can verify against (confirm the key
      format/encoding matches what monocypher expects)
- [ ] Confirm the trust-boundary design already reasoned about in
      commit `e9e29e3`'s message (boot-signing key separate from the
      package-signing key) is actually implemented, not just narrated

### 2.3 — Two-directions proof (same bar as every other crypto check
      in this project — LUKS, package signing, Boot Verifier PD)
- [ ] Sign a real test image with the real boot key → verify returns
      OK
- [ ] Corrupt one byte of a signed image → verify returns
      `WLT_BOOT_ERR_SIGNATURE`, not OK
- [ ] Corrupt the signature itself (valid image, tampered sig) →
      also rejected
- [ ] Paste real command output for all three, not a summary

---

## Workstream 3 — Desktop UX completion

*(Sequence after Workstream 1 if running in the same session — both
touch build infrastructure that can conflict if run truly
concurrently. If Workstream 1 stalls at 1.2, it's safe to move to
Workstream 3 while 1's blocker is documented for a future session.)*

### 3.1 — Right-click context menus (FR-GUI-11)
- [ ] A real, minimal menu surface — reuse the existing Wayland-
      client-or-compositor-overlay pattern already established for
      the launcher/taskbar, don't invent a new rendering approach
- [ ] At least one real, working use case: either a desktop right-
      click (e.g. "New folder", "Refresh") or a window title-bar menu
      (e.g. "Close", "Minimize", "Maximize") — pick whichever is less
      code to wire given what already exists in compositor.c's window
      management
- [ ] Real screenshot showing the menu open and a click actually
      triggering its action

### 3.2 — Drag and drop (FR-GUI-12)
- [ ] File-manager-to-desktop OR file-manager-internal drag/drop
      (pick one — internal is likely simpler given the existing file
      manager code)
- [ ] Real test: drag a file, confirm the drop actually moves/copies
      it (check the filesystem state before and after, not just that
      the UI animation played)

### 3.3 — Notifications system (FR-GUI-08)
- [ ] A real surface other components can push a message to (a
      Wayland overlay, toast-style or similar — reuse existing
      rendering patterns)
- [ ] Explicit design decision, documented in the commit message: does
      Guardian's existing Kairos-specific alert path (already proven:
      `[kairos] GUARDIAN ALERT: ...`) ALSO push here as the general
      OS-level equivalent, or is that a deliberately separate channel?
      Reason about it rather than silently duplicating or silently
      leaving it Kairos-only.
- [ ] Real test: trigger one real notification (reuse the existing
      Guardian synthetic test-alert trigger if that's the fastest real
      event source) and confirm it renders on the new surface

---

## Workstream 4 — Real package repository hosting

*(Fully independent of Workstreams 1-3 — safe to run in true parallel
with any of them.)*

### 4.1 — Serve the existing repo for real
- [ ] `packages/repo/` (3 packages + index, already real and
      committed) gets served over an actual reachable URL — temporary/
      test hosting is fine, the point is a genuine network fetch path,
      not proving anything new about the packages themselves
- [ ] Document exactly how it's hosted and the URL/method, since this
      needs to be reproducible for whoever picks this up next

### 4.2 — Wire wlt-pkg to fetch from it
- [ ] Update wlt-pkg's install flow to pull from the real remote repo
      instead of (or in addition to) the local path it currently uses
- [ ] Explicit decision: does this route through Tor, per this
      project's stated network policy? Check whether that's already
      assumed anywhere in wlt-pkg's existing code, and confirm the
      fetch doesn't silently bypass Tor the way the AI egress bug did
      earlier in this project (audit finding E-C1/C2) — that's the
      second time this exact class of bug has been worth checking for
      explicitly in this project, so check it directly rather than
      assuming the fix pattern from last time generalizes automatically

### 4.3 — Real end-to-end test
- [ ] Fetch, verify, and install one of the 3 existing packages from
      the real remote repo (not the local filesystem path)
- [ ] Confirm signature verification still works against the remote
      fetch: a genuine package installs, a deliberately tampered
      remote copy is rejected — same two-directions bar as every
      other crypto check
- [ ] Raw command output for both directions, not a summary

---

## Reporting

One report at the end, covering all four workstreams. For each
completed subtask: the raw verification output (grep/wc/git show/
serial log excerpt), not a restated claim. For anything genuinely
blocked or out of time: exact status and exact blocker, same honesty
standard as every report this project has been held to — a real
partial result on Workstream 1 in particular (this has stalled before)
is a legitimate and expected possible outcome, not a failure to hide.
