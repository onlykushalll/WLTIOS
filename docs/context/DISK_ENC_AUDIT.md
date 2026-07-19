# Disk Encryption + Amnesia Audit (2026-07-20)

## Methodology
Same discipline as the orphaned code audit: grep for source files,
`wc -l` for real line counts, compile check, grep for stubs/TODOs.
No remembered numbers — all from fresh grep/wc output.

## Findings

### 1. LUKS Disk Encryption — STUBS ONLY, NO IMPLEMENTATION

**Source:** `src/userspace/wlt-crypto/crypto.c` (310 lines)

```c
wlt_crypto_luks_format(const char *device, const char *key_id)
{
    WLT_CRYPTO_LOG_ERR("luks_format: not built (requires luks.c module)");
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}
```

All four LUKS API functions (`luks_format`, `luks_open`, `luks_close`,
`luks_status`) are stubs that return `WLT_CRYPTO_ERR_UNSUPPORTED`.

**Missing files (referenced in crypto.c but never written):**
- `luks.c` — LUKS2 header format/parse implementation
- `dm_crypt.c` — dm-crypt device-mapper interface

**Verdict:** The earlier audit's claim "disk encryption: code exists, never
verified" is incorrect. Only API stubs exist. The implementation was never
written. The spec (wlt-crypto.md) describes the design, but no code follows it.

### 2. File-Level Encryption — REAL IMPLEMENTATION

**Source:** `src/userspace/wlt-crypto/file_crypto.c` (496 lines)

Real streaming per-file encryption with WLT1 framing format. Uses:
- libsodium (optional, falls back to manual implementation)
- Chunk-by-chunk encrypt/decrypt
- Atomic temp file + rename (crash-safe)
- Memory zeroing (sodium_memzero or volatile pointer)

**Verdict:** Real, not stubs. Properly implements per-file encryption.
Has a test file reference (`tests/userspace/file_crypto_test.c`).

### 3. Secure Delete — REAL IMPLEMENTATION

**Source:** `src/userspace/wlt-crypto/secure_delete.c` (291 lines)

Real shredding implementation:
- 3-pass overwrite (random, 0x00, 0xFF) + final zero pass
- 1MB buffer for efficiency
- Refuses symlinks and non-regular files
- Uses /dev/urandom (or libsodium CSPRNG) for random pass

**Security note from the source:** "On modern flash and copy-on-write
filesystems, overwrite-based shredding is not a strong guarantee — the
storage device may remap blocks transparently."

**Verdict:** Real, not stubs. Properly implemented with honest security caveats.

### 4. Key Vault PD — VERIFIED REAL (2026-07-12)

**Source:** `src/userspace/wlt-crypto/keyvault.c` (96 lines)

Verified via seL4 multi-PD ping/pong test on 2026-07-12. The Key Vault PD
holds encryption keys and performs crypto operations inside a seL4 isolation
partition. Keys never leave the PD.

**Verdict:** Real and verified. But nothing currently USES it for disk
encryption (because luks.c/dm_crypt.c don't exist).

### 5. Kill Switch — VERIFIED REAL (earlier phase)

**Source:** `src/userspace/wlt-net/kill_switch.c`

Verified real in an earlier phase: route-based, bidirectionally reversible,
audit-logged. Triggers on Tor disconnect, blocks all network traffic.

**Verdict:** Real and verified. Network-level only — does NOT wipe disk keys
or trigger kexec.

### 6. kexec (kernel-space key wipe) — NO CODE EXISTS

Grep for "kexec" across `src/` returns only `src/userspace/wlt-defender/include/vmlinux.h`
(BPF header, not implementation). No kexec system call wrapper, no kexec load
function, no kexec exec.

**Verdict:** Does not exist. The "RAM wipe: userspace-only, can't wipe
kernel-space LUKS keys" gap is real and completely unaddressed.

### 7. RAM Wipe — NO CODE EXISTS

Grep for "ram.wipe", "memory.wipe", "secure.wipe" returns nothing in `src/`.
The amnesia feature is currently tmpfs-only: all writes go to RAM (tmpfs),
which is lost on reboot. But there is no ACTIVE wiping of kernel-space
secrets (dm-crypt keys in kernel keyring, page cache, etc.).

**Verdict:** Does not exist. Amnesia is passive (tmpfs), not active (wipe).

### 8. Spec — EXISTS BUT INCOMPLETE

**File:** `docs/specs/wlt-crypto.md`

Describes LUKS2 + dm-crypt + Key Vault PD integration, file encryption,
secure delete. No mention of kexec or kernel-space key wipe. No separate
`wlt-disk-encryption.md` spec file.

## Summary Table

| Component | Lines | Status | Verified? |
|-----------|-------|--------|-----------|
| LUKS (crypto.c stubs) | 310 | STUBS — returns UNSUPPORTED | No (can't — no impl) |
| luks.c | 0 | DOES NOT EXIST | N/A |
| dm_crypt.c | 0 | DOES NOT EXIST | N/A |
| file_crypto.c | 496 | REAL implementation | No (compiles, not runtime tested) |
| secure_delete.c | 291 | REAL implementation | No (compiles, not runtime tested) |
| keyvault.c | 96 | REAL | Yes (seL4 ping/pong, 2026-07-12) |
| kill_switch.c | ~200 | REAL | Yes (earlier phase) |
| kexec code | 0 | DOES NOT EXIST | N/A |
| RAM wipe code | 0 | DOES NOT EXIST | N/A |

## What This Means for Sections A/B/C

**Section A (Real LUKS):** Can't "verify existing code" — need to either:
- Write luks.c + dm_crypt.c from scratch (large effort), OR
- Use `cryptsetup` as an external tool (pragmatic, testable now)

**Section B (kexec RAM wipe):** Need to write kexec wrapper code from scratch.
The `kexec_load` + `kexec_exec` syscalls exist in Linux but no WLTIOS code
calls them.

**Section C (end-to-end):** Depends on A + B. The kill switch is already
verified — it just needs to also trigger the disk key wipe + kexec.
