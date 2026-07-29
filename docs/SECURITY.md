# WLTIOS — Security Architecture (Consolidated Reference)

> **Supersedes:** original `docs/SECURITY.md`, `docs/PRIVACY.md`,
> `docs/security/security-whitepaper.md`, `docs/security/audit-results.md`,
> and the security-relevant portions of `docs/context/DISK_ENC_AUDIT.md`,
> `MEMORY_SAFETY_AUDIT.md`/`RACE_AUDIT.md` (wlt-defender), and the ADRs.
> All archived under `_ARCHIVED_2026-07-29`. This document states what is
> **independently verified against source/commits**, separately from what
> is still aspirational.
> **Reporting a vulnerability:** use GitHub's private vulnerability
> reporting (Security tab). No dedicated security-contact email is set up
> yet — this was true in the original doc and remains true.

---

## 1. Big-picture honesty check

The original security whitepaper and SECURITY.md described a system where
"even if Linux is fully compromised, Tor/keys/firewall remain safe" as
**present-tense fact**. As of this pass, that guarantee is **not yet real**:

- The desktop (where a user would actually be exposed to attack) runs on
  Track 2 - a Linux kernel booted directly in QEMU with **no seL4
  underneath it at all** (see `docs/ARCHITECTURE.md` §1). seL4 isolation
  protects nothing on that track today.
- Firewall PD (the component that would enforce Tor-only routing at the
  seL4 level, independent of Linux) does not exist yet.
- TorPD (Tor daemon isolated in a second VM) has a working two-VM boot as
  of 2026-07-25 but the actual isolation/relay behavior is unconfirmed in
  this pass.
- Key Vault PD's isolation is real and verified (ping/pong across the seL4
  boundary, 2026-07-12) but nothing on the Track 2 desktop currently routes
  its actual disk-encryption keys through it end-to-end (LUKS today derives
  its key material through a userspace path - see §3).

Treat every "seL4 guarantees X" sentence elsewhere in this repo's history
as a target, not a current property, unless this document says otherwise.

---

## 2. Verified boot

**Real, as of commit `c169cb2` (2026-07-28).** `src/boot/wlt-boot/verify.c`
was independently re-read for this consolidation: it calls monocypher's
`crypto_eddsa_check(sig, pubkey, data, data_size)` and is fail-closed if the
public key is all-zero (unconfigured). Monocypher v4.0.2 is vendored at
`packages/monocypher/` (2,988 + 321 lines, single-file C, no deps). The
commit message and diff both claim a two-directions proof: valid signature
accepted, corrupted image rejected, tampered signature rejected - this
consolidation did not re-run that proof, only confirmed the code path
matches the claim.

**History of this specific code path (why it's worth stating precisely):**
an earlier version of `wlt_verify_ed25519` was a **fail-open stub** that
returned `WLT_BOOT_OK` unconditionally regardless of input - a silent
bypass that was live for some period before being caught and fixed
(commit `1ba724e`, "verified boot fail-OPEN bug fixed to fail-CLOSED").
This is the canonical example, cited throughout this project's own docs, of
why status claims here get re-verified against source rather than trusted.

**dm-verity** (block-level integrity for the active partition): manifest
fields, parser, and kernel config (`CONFIG_DM_VERITY=y` etc.) landed
2026-07-23 (`84c8df4`). The initrd-side `veritysetup open` sequence and a
real QEMU boot proof are explicitly still open per the update spec.

---

## 3. Disk encryption (LUKS)

**Status has changed materially since the last audit that called this
"stubs only."** `docs/context/DISK_ENC_AUDIT_ARCHIVED_2026-07-29.md`
(2026-07-20) found all four LUKS API functions in `crypto.c` returning
`WLT_CRYPTO_ERR_UNSUPPORTED` unconditionally, and `luks.c` not existing at
all. Independently re-checked for this consolidation (2026-07-29):
`src/userspace/wlt-crypto/luks.c` **exists**, and `crypto.c` now dispatches
`wlt_crypto_luks_format` / `_open` / `_close` to real `_impl` functions in
that file rather than stub returns. Git history shows the real
implementation landing 2026-07-21 (`1045770`/`d687790`, initially lost to a
merge conflict and re-added `f6446fe`), then wired end-to-end and verified
2026-07-22 (`4214a32`, `ae2e6c3`).

- **File-level encryption** (`file_crypto.c`, 496 lines) and **secure
  delete** (`secure_delete.c`, 291 lines, 3-pass overwrite + zero pass,
  refuses symlinks/non-regular files, documents the flash-remapping caveat
  honestly in its own source comments) were already real prior to this and
  remain so.
- **kexec-based kernel-space key wipe** and **active RAM wipe of
  kernel-space secrets**: confirmed **do not exist** as of the 2026-07-20
  audit; not re-verified as added since. Amnesia today is passive (tmpfs),
  not an active secure wipe of dm-crypt keys held in the kernel keyring.
- **luksOpen + mount, and full amnesic kexec-wipe**, need root / a
  pre-loaded wipe kernel that the WSL2 dev environment can't provide - a
  real environment limitation, not a code gap, per the project's own
  characterization (independently plausible given the "no sudo in WSL2"
  constraint documented throughout).

---

## 4. App sandboxing and permissions

**Independently re-read for this consolidation** (`src/userspace/wlt-sandbox/launch.c`,
2026-07-29): confirms the two specific claims in the project's own PRD:

1. **Fail-closed permission-manager init.** If `wlt_perms_init()` fails
   (e.g. read-only `/var` during amnesic boot), `wlt_sandbox_launch()`
   returns `WLT_SANDBOX_ERR_PERMISSION` and refuses to launch, with a log
   line citing "ultrareview C1" - this was a fix for a prior fail-**open**
   bug where an I/O error silently disabled all permission enforcement for
   every app launch. Confirmed real in source, not just claimed.
2. **Only `WLT_PERM_NETWORK` is actually gated.** The permission check in
   `launch.c` only inspects the network grant (`WLT_PERM_NETWORK`) and
   denies launch if network is requested and the grant is `DENY`. The
   other ~11 declared permission categories (camera, microphone, location,
   filesystem, etc., per `docs/specs/wlt-permissions_ARCHIVED_2026-07-29.md`)
   are registered/persisted/audited by `wlt-permissions` (itself a clean,
   zero-stub module per the orphaned-code audit) but **nothing in the
   reviewed launch path actually gates them**.

`wlt-permissions` itself (manifest parsing, grant/deny logic, persistence,
audit log, privacy badges) is real, per the orphaned-code audit (1,246
lines, zero stubs, "cleanest orphaned code of the batch").

`wlt-ai/tools.c` (the AI's tool-execution surface) is a **known gap,
independently re-confirmed by source grep in this pass**: `wlt_tool_run_command`
still calls `popen(cmd, "r")` (line ~268) to run arbitrary LLM-supplied
shell text, gated only by an 11-entry denylist - the project's own PRD
correctly characterizes a denylist over `/bin/sh -c` as "fundamentally the
wrong shape." `wlt_tool_change_setting`, by contrast, was rewritten
(confirmed via grep) to use `fork()`+`execv()` with no shell in the loop -
this was a real, verified fix (commit `ae2e6c3`, "shell injection in AI
tool execution"). The AI's entire tool-execution surface also runs outside
the `wlt-sandbox`/`wlt-permissions` enforcement every other app launch goes
through - adding a new `wlt_perms_id_t` category to route it through the
same system was an active track as of the PRD's last update and its current
status was not re-confirmed in this pass.

---

## 5. Guardian (wlt-defender) — endpoint security engine

See `docs/AI_KAIROS.md` for the full picture (Guardian and Kairos share a
lot of context and are documented together there). Summary relevant to
this security doc:

- Architecture is deliberately **not LLM-first** - a deterministic 5-layer
  core (YARA signatures, eBPF behavioral rules, file-integrity monitoring,
  a small isolation-forest anomaly model, network IDS) plus an optional,
  on-demand LLM consultant. This design decision is documented and
  reasoned, not just asserted (`WLTIOS_GUARDIAN_HANDOFF_ARCHIVED_2026-07-29.md`).
- The project's own audits (`MEMORY_SAFETY_AUDIT.md`, `RACE_AUDIT.md`,
  `THREAT_MODEL.md`, all archived, content folded in here) found **no
  critical memory-safety issues** in the audited files, and a real
  ABBA-deadlock-prevention mechanism (the "Bug 8 fix": copy the event-bus
  subscriber list, release the bus lock, then invoke callbacks) that this
  consolidation did not re-derive from scratch but which reads as a
  legitimate, specific technical finding rather than a generic "looks
  fine" pass.
- **eBPF hook attachment status is genuinely ambiguous across the docs and
  needs care.** `docs/context/BPF_LOADING_ANALYSIS_ARCHIVED_2026-07-29.md`
  (2026-07-01) documents BPF object load failing outright. The same day,
  commit `8b146ae` fixed the load (via a serial-port-transferred real
  `vmlinux.h`) but tracepoint *attach* still failed with `-ENOENT` because
  `tracefs` wasn't mounted in the guest. Re-checked for this consolidation:
  `src/userspace/wlt-init/init-minimal.c` **does** mount `tracefs` at
  `/sys/kernel/tracing` (non-fatal on failure), and
  `src/userspace/wlt-defender/src/ebpf_engine.c` has a real
  `bpf_program__attach()` call path, gated by a runtime `CAP_BPF` check
  that reports "degraded mode" when the capability isn't available (this
  is expected and correct behavior in a non-root/no-CAP_BPF environment,
  not a bug). Whether a real WSL2-as-root QEMU boot currently shows all
  declared hooks actually attached (vs. degraded mode) was **not
  independently re-run in this pass** - MASTER_CONTEXT's 2026-07-04 entry
  claims "23 BPF hooks attached (verified via vmlinux.h transfer)," which
  post-dates the fix commit and is plausible, but should be re-verified
  with a fresh serial log before being treated as current fact.

---

## 6. Kill switch, MAC randomization, amnesia

All three were independently corroborated across multiple independent
audit passes in the project's own history (not just a single self-report)
and are treated as real:

- **Kill switch:** route-based (`SIOCDELRT`/`SIOCADDRT` on the default
  gateway), not iptables-dependent - built specifically because the 64-bit
  guest's kernel lacks the `ip_tables.ko` module. Verified via QEMU serial
  showing route deletion and restoration (`PHASE5_FINAL_STATUS_ARCHIVED_2026-07-29.md`).
  This is a Linux-userspace enforcement mechanism, not seL4/Firewall-PD
  enforcement (see §1).
- **MAC randomization:** verified across 3+ boots producing different MAC
  addresses each time.
- **Amnesia:** `/tmp`, `/var`, `/run`, `/home` mounted tmpfs; file-write/reboot
  test confirmed content is gone after reboot. This is passive amnesia only
  (see §3 for the active-wipe gap).

---

## 7. Privacy claims (from the original `docs/PRIVACY.md`)

The "zero telemetry" and "no data leaves the device without explicit
action" claims are architecturally plausible (no telemetry code was found
in any audit) but were not the subject of a dedicated search for outbound
calls in this consolidation pass - treat as likely-true-by-absence-of-evidence
rather than independently proven. The cloud-AI privacy pipeline (PII
stripping, Tor routing for cloud API calls) described in the original
privacy policy corresponds to real code in `wlt-ai/privacy_pipeline.c`
(261 lines, no stubs, per the orphaned-code audit) - but whether cloud mode
is ever actually exercised (no cloud API key has been configured in any
verified session) is a separate question from whether the pipeline code
itself is real.

---

## 8. Static audit history (for reference, not current guarantee)

The original `docs/security/audit-results.md` (2026-06-22) covered 45
`.c` files from a much earlier snapshot (pre-compositor, pre-Kairos,
pre-Guardian) and found 15 build-blocking issues, all fixed, and zero
buffer-overflow/format-string/hardcoded-credential patterns in that
snapshot. It is not a current audit of today's much larger codebase and
should not be quoted as covering current code.
