# WLTIOS — Boot Sequence and A/B Updates (Consolidated Reference)

> **Supersedes:** `docs/specs/wlt-boot.md`, `docs/specs/wlt-init.md`,
> `docs/specs/wlt-update.md`, `docs/specs/key-vault-pd.md`,
> `docs/specs/boot-verifier-pd.md`. Archived under `_ARCHIVED_2026-07-29`.

---

## 1. Boot chain (target design vs. what's real)

Target design (`wlt-boot.md`): UEFI firmware -> WLT-Boot (verifies seL4
signature) -> seL4 -> Linux guest -> WLT-Init (PID 1) -> desktop.

**Real, today:** WLT-Boot has a genuinely working Ed25519 verified-boot
check (see `docs/SECURITY.md` §2). Track 2 (the desktop you'd actually use)
boots a Linux kernel **directly** in QEMU, bypassing seL4 and therefore
bypassing WLT-Boot's verification entirely for that path (see
`docs/ARCHITECTURE.md` §1). Track 1 (seL4 multi-PD) has its own,
separate, verified boot-to-Buildroot-login path.

## 2. WLT-Init (PID 1)

Real and working on both tracks it's used on: mounts `/proc`, `/sys`,
`/dev`, tmpfs paths for amnesia; brings up `tracefs` for Guardian's eBPF
hooks (confirmed in `init-minimal.c`, see `docs/SECURITY.md` §5); starts
Guardian as PID 2/3 depending on the doc generation (`watchdog` at PID 2,
`guardian` daemon at PID 3, per `58ca83a`). No malloc in PID 1 is an
enforced coding-style rule (`docs/PROCESS_METHODOLOGY.md`) and was
specifically checked and fixed early in the project's history per
`docs/context/MASTER_CONTEXT_ARCHIVED_2026-07-29.md`'s bug list.

## 3. Key Vault PD persistence design

The original spec's TPM-sealed bootstrap design (seal the LUKS master key
to TPM PCR values at install time, unseal it before Linux starts, map it
into Key Vault PD memory) remains a **design only** - no TPM emulation is
configured in this build (see `docs/SECURITY.md` §3, §5 on the
`TPM_KEY_SEALING.md` finding for the forensic log, which faces the
identical TPM-unavailability problem). What's real is the Key Vault PD's
seL4 isolation itself (ping/pong verified 2026-07-12) and an FD-based key
delivery client (`abe3d84`, 2026-07-23) written for `wlt-update`'s
dm-verity work - the bridge daemon on the PD side of that connection is
still open work.

## 4. A/B update system (`wlt-update`)

This is one of the most mature, most carefully audited-and-re-audited parts
of the project. `docs/context/AB_UPDATE_AUDIT_ARCHIVED_2026-07-29.md`
(read-only audit, `-fsyntax-only` compiles, real `wc -l`) found:

- **4 real C files** (`update.c` 711 lines, `partition.c` 441, `config.c`
  283, `verify.c` 232 - 2,088 lines total including headers/Makefile), not
  the 6-file Rust layout the spec describes. `download.c`/`transaction.c`
  don't exist as separate files - their logic lives inside `update.c`.
- **Zero TODOs/stubs/placeholders** across all 4 files - a real, complete
  implementation, directly contradicting an earlier `FILETREE.md` claim
  that this module was "skeleton, no actual download/install pipeline."
  (`FILETREE.md` is now archived; this is exactly the kind of false status
  claim this consolidation was tasked with catching.)
- **Real A/B partition scheme**: two LUKS2-mapped slots
  (`/dev/mapper/wlt-root-{a,b}`), atomic INI boot-config
  read/write (temp-file + rename), a rollback path that **refuses to flip
  to a slot marked `failed`** (a genuine no-bricking guarantee, confirmed
  by reading the actual refusal logic, not just the comment above it).
- **Ed25519 blob-signature verification is real** (streaming
  `crypto_sign_init/update/final_verify`, so 500 MB packages never need to
  fit in RAM) but was, as of the 2026-07-20 audit, **inert on the WSL2
  build host** because `libsodium-dev` (headers) weren't installed - only
  the runtime `.so` was present, so `HAVE_SODIUM` was undefined at compile
  time and every verify call returned `-13` (unsupported). This blocked the
  entire install pipeline from ever succeeding, since `wlt_update_install()`
  calls verify before touching any partition. **Not re-checked in this
  pass whether libsodium-dev has since been installed** - re-verify with
  `pkg-config --exists libsodium` before assuming installs work.
- **No `wltupdate` CLI binary exists** - the Makefile explicitly builds
  only the library; the spec's user-facing `wltupdate check/install/
  rollback/...` commands are not currently deliverable to a user.
- **Not wired into `build-all.sh`** as of the 2026-07-20 audit - the
  library is real but wasn't being built/bundled into the actual rootfs at
  that time. Given the subsequent flurry of `wlt-update` commits (see
  below), this may have changed; not re-confirmed in this pass.

### Security pipeline layered on top since the audit (per `git log`, spot-checked)

All of the following are real commits with real diffs, confirmed via
`git show --stat`, landing 2026-07-21 through 2026-07-23:

| Layer | What | Commit |
|---|---|---|
| R2 | Anti-rollback monotonic counter, HMAC-integrity-protected, tamper-evident (refuses on HMAC mismatch rather than silently resetting) | `3d3a26e` |
| R5 | Signed manifest binds signature to `{version, rollback_index, target_slot, sha256(blob), verity fields, key_id}` | `7daed48` |
| R6 | dm-verity manifest fields + kernel config (foundation only - initrd `veritysetup open` sequence and a real boot proof are explicitly still open) | `84c8df4` |
| R7 | Key chain with rotation (multiple signing keys with validity windows, so a compromised key can be retired without reflashing) | `db4f8d0`, completed `d791ef4` |
| - | KeyVaultPD bridge, FD-based key delivery client (MVP) | `abe3d84` |

The spec document for this (`docs/specs/wlt-update_ARCHIVED_2026-07-29.md`)
was itself written *as* this pipeline was completed (2026-07-23,
`736a7a2`), and its content is folded into this section rather than
duplicated.

## 5. Rollback / boot-attempt tracking

Real, per the audit: `wlt_boot_config_increment_attempts()` flips `failed =
true` after 3 failed boots (`WLT_UPDATE_MAX_BOOT_ATTEMPTS`), and rollback
refuses to flip to an already-failed slot. Auto-rollback triggered by
dm-verity failure specifically needs a bootloader handshake that is
explicitly listed as not yet implemented.

## 6. Open items (as last documented, not re-verified further in this pass)

- Initrd-side `veritysetup open` + real QEMU boot verification for dm-verity
- Auto-rollback on verity failure (needs bootloader handshake)
- KeyVaultPD bridge daemon (server side of the FD-based key delivery)
- `wltupdate` CLI binary
- Confirming `libsodium-dev` is actually available at build time
