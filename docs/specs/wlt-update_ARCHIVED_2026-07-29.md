# WLT-Update — A/B Update System Specification

## Overview

WLT-Update is the atomic A/B update agent for WLTIOS. It fetches version
manifests over a Tor SOCKS5 tunnel, downloads Ed25519-signed packages,
verifies them, and drives the atomic install onto the inactive partition.

This document specifies the security pipeline as implemented (commits
3d3a26e, 7daed48, 84c8df4, db4f8d0, d791ef4).

## Threat Model

- **Network attacker**: controls the network, can MITM, replay, or serve
  crafted packages. Defeated by: Tor transport (confidentiality), Ed25519
  signature (integrity), anti-rollback counter (replay), signed manifest
  (binding).
- **Physical attacker with stolen disk**: has the disk image, wants to
  decrypt. Defeated by: LUKS2 (see wlt-crypto spec), KeyVaultPD key
  delivery (research-H).
- **Runtime root attacker**: has root once, wants to persist a trojan.
  Defeated by: dm-verity on the active slot (R6, in progress).

## Security Pipeline (layers, in verify order)

### Layer 1: Ed25519 Blob Signature (existing)

`wlt_update_verify_file()` streams the package blob through
`crypto_sign_update` and verifies the detached Ed25519 signature
(`crypto_sign_final_verify`). Refuses any package whose signature doesn't
validate. Streamed so 500 MB packages never need to fit in RAM.

### Layer 2: Anti-Rollback Counter (R2, commit 3d3a26e)

`rollback.h`/`rollback.c` — a tamper-evident monotonic counter at
`/var/lib/wlt/rollback.counter`:

- Format: `<decimal-counter>\n<hex-HMAC-SHA256>`
- HMAC key: build-time constant (clearly marked as needing KeyVaultPD
  delivery for the full design — prevents filesystem-exploit tampering,
  not root+binary extraction).
- `wlt_rollback_load()`: verifies HMAC; returns ERR_IO on mismatch
  (tamper-evident: refuse rather than silently resetting to 0).
- `wlt_rollback_check(package_index)`: returns DOWNGRADE if
  `package_index < stored`.
- `wlt_rollback_bump(new_index)`: stores if `new_index > stored`
  (monotonic, never decreases).
- Atomic write: O_EXCL|O_NOFOLLOW|fsync file+dir|rename.

Checked at `wlt_update_check()` (before download) AND re-checked at
`wlt_update_install()` (defense-in-depth, after manifest verify).

### Layer 3: Signed Manifest (R5, commit 7daed48)

`wlt_update_verify_manifest()` + `wlt_update_compute_sha256()` in
`verify.c` — binds the signature to `{version, rollback_index,
target_slot, sha256(blob), verity_root_hash, verity_salt, verity_size,
key_id}`:

- The manifest is a small KV file alongside the package:
  `version=`, `rollback_index=`, `target_slot=`, `sha256=`,
  `verity_root_hash=`, `verity_salt=`, `verity_size=`, `key_id=`.
- The manifest has its own detached Ed25519 signature (`.manifest.sig`).
- `wlt_update_verify_manifest()`: verifies the manifest signature, parses
  the fields.
- `wlt_update_compute_sha256()`: streams the blob through
  `crypto_generichash` (SHA-256), checks it matches the manifest.
- At install time: check target_slot matches the inactive slot; re-check
  rollback_index against the stored counter.

If no `.manifest` exists, logs a warning and proceeds (backward-compatible).

### Layer 4: dm-Verity (R6, commit 84c8df4 — foundation)

The active slot is mounted read-only with block-level integrity verification
via dm-verity (per research-P-dm-verity):

- **Build time**: `veritysetup format` generates the Merkle tree over the
  ext4 slot image. The root hash + salt + hash-area size go into the
  signed manifest (R5 fields).
- **Boot time** (initrd, not yet implemented): reads the root hash from
  the signed manifest, runs `veritysetup open` before `switch_root`, mounts
  the slot read-only. `/var` is a separate mount; `/etc` is an overlayfs
  over the read-only verity `/etc`.
- **Kernel config**: `CONFIG_DM_VERITY=y`, `DM_VERITY_FEC=y`,
  `OVERLAY_FS=y`, `EXT4_FS=y` (added to `wlt-kernel.config`).
- **On verity failure**: halt (auto-rollback needs the bootloader
  handshake, research-I G8 — not yet implemented).

Status: manifest fields + parser + kernel config landed (84c8df4). The
initrd `veritysetup open` sequence + build pipeline + boot verification
are QEMU-gated follow-up.

### Layer 5: Key Chain + Rotation (R7, commits db4f8d0 + d791ef4)

`keyring.h`/`keyring.c` — multiple Ed25519 signing keys with key-ids +
validity windows, so a compromised key can be retired without reflashing:

- Keyring file: `/etc/wlt/keys/update-keyring.bin` (binary, magic 'WLTK').
- Each entry: `key_id` + 32-byte `pubkey` + `not_before` + `not_after`.
- `wlt_keyring_load()` / `wlt_keyring_lookup(kr, key_id, now, out_pubkey)`.
- `wlt_update_verify_manifest()`: after parsing, if `manifest.key_id` is
  non-empty, calls `wlt_keyring_find(key_id)` and re-verifies the
  signature with the keyring's pubkey. A manifest signed by a retired
  key (not in the keyring, or expired) is refused.

## Install Flow (wlt_update_install)

1. Acquire flock (WLT_UPDATE_ERR_BUSY on contention).
2. Open the package ONCE (O_NOFOLLOW) — pass `/dev/fd/N` to verify + tar
   (closes TOCTOU F1).
3. Free-space check BEFORE format (C-H1/C-H2).
4. Verify blob Ed25519 signature (Layer 1).
5. If `.manifest` exists: verify manifest sig, check blob SHA-256 matches,
   check target_slot, re-check rollback_index (Layers 2+3+5).
6. Format + mount inactive slot.
7. Extract package (tar via execve, not system()).
8. Save boot config atomically (O_EXCL|O_NOFOLLOW|fsync file+dir|rename).
9. Bump anti-rollback counter (Layer 2).
10. Set pending_reboot; write status.

## Verified Boot Flow (R6, pending initrd)

1. Bootloader selects active slot, passes to kernel.
2. Initrd reads the signed manifest from `/boot/wlt-X.manifest`.
3. Ed25519-verifies the manifest (reuses verify.c).
4. Reads `verity_root_hash` + `verity_salt` + `verity_size`.
5. `veritysetup open <data_dev> wlt-verity-X <root_hash> --hash-offset <off>`.
6. `mount -o ro /dev/mapper/wlt-verity-X /mnt/real-root`.
7. Mount `/var` (state partition); overlayfs `/etc` over the RO verity `/etc`.
8. `exec switch_root /mnt/real-root /sbin/init`.

## File Locations

- `/var/lib/wlt/rollback.counter` — anti-rollback counter (R2)
- `/var/lib/wlt/update.status` — update status (pending_version, rollback_index)
- `/boot/wltios.conf` — boot config (A/B slot selection)
- `/etc/wlt/keys/update-pubkey.bin` — single Ed25519 pubkey (legacy)
- `/etc/wlt/keys/update-keyring.bin` — keyring with rotation (R7)
- `/etc/wlt/keys/update-pubkey.bin` — fallback when no keyring/key_id

## Open Items (honestly deferred)

- **R6 initrd**: `veritysetup open` sequence + build pipeline + QEMU boot
  verification (needs kernel rebuild + boot).
- **R6 auto-rollback on verity failure**: needs bootloader handshake (G8).
- **KeyVaultPD bridge daemon**: the `wlt_keyvault_get_volume_key_fd()`
  client is done (abe3d84); the bridge daemon that reads from
  `/dev/virtio-port/wltkv` needs seL4 KeyVaultPD + virtio-console.
- **dm-verity FEC + kernel-side root-hash sig** (`DM_VERITY_VERIFY_ROOTHASH_SIG`):
  ideal hardening (research-P §4).
