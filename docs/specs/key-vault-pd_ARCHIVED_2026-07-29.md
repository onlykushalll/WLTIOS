# Spec: Key Vault PD (seL4 Partition)

> **Status:** Draft (ready for implementation)
> **Phase:** 3 (Month 13-14)
> **Dependencies:** seL4, Microkit
> **ADR references:** ADR-02 (seL4 base), ADR-04 (Microkit)
> **Research:** docs/context/RESEARCH_SEL4_PARTITIONS.md

## Purpose

The Key Vault PD is an seL4 Protection Domain that stores all encryption keys and performs cryptographic operations. **Keys never leave this partition.** When an app needs to encrypt or decrypt, it sends data to the Key Vault via IPC; the Key Vault does the crypto and returns the result.

## The Critical Insight

In Tails, encryption keys live in `/etc/` or `/home/`. If Linux is hacked, keys are stolen. In WLTIOS, keys live in the Key Vault PD, isolated by seL4. **Even if Linux is fully compromised, the attacker cannot read the keys.** seL4's formal proofs guarantee this.

```
App (in Linux) ──IPC──→ Key Vault PD ──→ performs crypto ──→ returns result
                              │
                         Keys stored here
                         (Linux cannot read)
```

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Encrypt request | WLT-Crypto (IPC) | Plaintext + key ID |
| Decrypt request | WLT-Crypto (IPC) | Ciphertext + key ID |
| Key generation request | WLT-Init (IPC) | Key type + ID |
| Tor identity key request | Tor VM (IPC) | Key ID |
| Authentication | User (via Auth PD) | Password/biometric |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Encrypted data | WLT-Crypto (IPC) | Ciphertext |
| Decrypted data | WLT-Crypto (IPC) | Plaintext |
| Key IDs (not keys) | Apps (IPC) | Key identifier string |
| Status | WLT-Init (IPC) | Key Vault operational |

## What's Stored

| Key Type | Purpose | Generated When |
|----------|---------|----------------|
| LUKS master key | Disk encryption | At install |
| User encryption key | Per-user file encryption | At user creation |
| Tor identity key | Tor's long-term identity | At first boot |
| Package signing key | Verify package signatures | Pre-installed |
| Update signing key | Verify OS update signatures | Pre-installed |
| AI memory key | Encrypt AI conversation history | At first boot |
| Per-app keys | App-specific encryption | On demand |


## Key Persistence Architecture

> **Added 2026-06-22** — Closes the circular dependency: "need Key Vault to decrypt disk, need disk mounted before Key Vault's Linux-side client can run."

### The Problem

PD memory is volatile — keys are lost on power off. But keys must survive reboots. The naive approach (store keys on disk, load at boot) creates a circular dependency:

```
Need Key Vault → to decrypt disk
Need disk mounted → before Key Vault's Linux-side IPC client can run
→ DEADLOCK
```

### The Solution: TPM-Sealed Bootstrap Key

The LUKS master key never lives on disk in plaintext. It is sealed at install time and unsealed at boot time **before Linux starts**.

### Bootstrap Sequence

```
┌─────────────────────────────────────────────────────────────┐
│ 1. INSTALL TIME (once, during OS installation)               │
│    → Generate LUKS master key (32 bytes, random)             │
│    → Seal it:                                                 │
│      Option A: TPM 2.0 seal (bind to PCR values)             │
│      Option B: Encrypt with user passphrase (Argon2id KDF)   │
│    → Store sealed blob at fixed offset on /dev/sda1 (ESP,    │
│      outside LUKS — the ESP is FAT32, unencrypted, but the   │
│      blob itself is encrypted so it's useless without the    │
│      TPM or passphrase)                                       │
│    → LUKS-encrypt /dev/sda2 (root) and /dev/sda3 (home)      │
│      using the master key                                     │
└─────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. BOOT TIME (every boot, before Linux)                      │
│    → WLT-Boot (UEFI) reads the sealed blob from /dev/sda1    │
│    → Unseals it:                                              │
│      Option A: TPM 2.0 unseal (auto, if PCR values match)    │
│      Option B: Prompt user for passphrase (UEFI input)       │
│    → Passes the raw LUKS master key to seL4 via a boot       │
│      parameter (memory region, not command line)              │
│    → seL4 maps this key into the Key Vault PD's private      │
│      memory at init time, BEFORE Linux starts                 │
└─────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. RUNTIME (after boot, while OS is running)                 │
│    → LUKS master key lives ONLY in Key Vault PD memory       │
│    → Never on disk in plaintext                              │
│    → Never in Linux kernel memory (except briefly during     │
│      dm-crypt setup, via wlt_keyvault_get_volume_key)        │
│    → All other keys (Tor identity, app keys, AI memory)      │
│      are generated at runtime, encrypted with a key derived  │
│      from the LUKS master key, and stored in an encrypted    │
│      key database on /dev/sda3 (user data partition)         │
│    → This key database is loaded into Key Vault PD at boot   │
│      (Linux reads the encrypted file, passes it to PD via    │
│      IPC; PD decrypts it using the LUKS master key already   │
│      in its memory)                                          │
└─────────────────────────────────────────────────────────────┘
```

### Why This Works

| Concern | Resolution |
|---------|-----------|
| Circular dependency | Broken: WLT-Boot (UEFI) unseals the key before Linux starts, passes it to seL4, seL4 maps it into PD memory. Linux never needs to "load" the key — it's already there. |
| Key on disk in plaintext | Never. The sealed blob on /dev/sda1 is encrypted (TPM or passphrase). The LUKS master key in PD memory is volatile (gone on power off). |
| Linux compromise | Useless. Even if Linux is fully owned, the attacker cannot read PD memory (seL4 isolation). The key database on disk is encrypted with the LUKS master key, which the attacker can't access. |
| TPM unavailable | Fallback to passphrase (Argon2id). User enters passphrase at UEFI prompt before seL4 starts. |
| Key rotation | Generate new master key, re-encrypt disk, re-seal. Old key wiped from PD memory. |

### Security Properties

- **Cold boot attack resistant**: Keys in PD memory, not Linux kernel memory. PD memory is a fixed, small region that seL4 controls.
- **Evil maid resistant**: TPM PCR binding detects firmware/bootloader tampering (TPM refuses to unseal if PCR values don't match).
- **Brute-force resistant**: Argon2id KDF (memory-hard) for passphrase fallback.
- **Forward secure**: Key database keys are generated at runtime (not pre-computed), so compromising the install-time master key doesn't compromise future runtime keys.

### What's NOT Stored in the Key Vault PD

| Item | Where it lives | Why |
|------|---------------|-----|
| User files | On disk (LUKS-encrypted) | Too large for PD memory |
| OS binaries | On disk (verified by Boot Verifier PD) | Too large for PD memory |
| Logs | On disk (in /var, inside LUKS) | Privacy via disk encryption, not PD |
| Browser cache | On disk (in /home, inside LUKS) | Privacy via disk encryption, not PD |

The Key Vault PD stores ONLY cryptographic keys and performs ONLY crypto operations. It is not a general-purpose secure storage — it is a **crypto coprocessor** isolated by seL4.

## Dependencies

- **seL4** (microkernel — provides isolation)
- **Microkit** (framework for defining PDs)
- **monocypher** (crypto library — works in bare-metal seL4 environment)

## What Depends on This

- WLT-Crypto (all crypto operations)
- WLT-Init (disk encryption unlock at boot)
- Tor VM (needs Tor identity key)
- WLT-Pkg (package signature verification)
- WLT-Update (update signature verification)

## API (IPC Messages)

```c
// Generate a new key, returns key ID (not the key itself)
wlt_Error wlt_keyvault_generate_key(wlt_KeyType type, char *key_id_out);

// Encrypt data (key stays in PD)
wlt_Error wlt_keyvault_encrypt(const char *key_id,
                                const uint8_t *plaintext, size_t plaintext_len,
                                uint8_t *ciphertext_out, size_t *ciphertext_len_out);

// Decrypt data (key stays in PD)
wlt_Error wlt_keyvault_decrypt(const char *key_id,
                                const uint8_t *ciphertext, size_t ciphertext_len,
                                uint8_t *plaintext_out, size_t *plaintext_len_out);

// Delete a key
wlt_Error wlt_keyvault_delete_key(const char *key_id);

// List key IDs (not keys themselves)
wlt_Error wlt_keyvault_list_keys(char *key_ids_out, size_t *count_out);
```

## Error Handling

| Error | Action |
|-------|--------|
| Unknown key ID | Return error, log attempt (possible attack) |
| Decrypt failed (wrong key) | Return error, log attempt |
| IPC from unauthenticated source | Refuse, log security event |
| Key storage corrupt | Attempt recovery from backup, log critical |
| Out of key storage space | Return error, log warning |

## Tests

| Test | How |
|------|-----|
| Generate key | Generate key, verify key ID returned |
| Encrypt + decrypt | Encrypt data, decrypt, verify matches original |
| Key isolation | Try to read key material directly, verify impossible |
| Wrong key decrypt | Decrypt with wrong key ID, verify failure |
| Unauthenticated IPC | Send IPC without auth, verify refused |
| Key persistence | Reboot, verify keys still accessible |

## Security

- **Keys never leave PD** — All crypto happens inside the partition
- **seL4 isolation** — Linux cannot read PD memory (formally verified)
- **No filesystem** — Keys stored in PD's private memory (not on disk)
- **Authentication required** — Only authenticated apps can request operations
- **Audit logging** — All crypto operations logged (what was encrypted, when — NOT the key or plaintext)
- **Memory zeroing** — All plaintext buffers zeroed after use

## Performance

| Metric | Budget |
|--------|--------|
| Encrypt/decrypt (small, < 1KB) | < 1ms |
| Encrypt/decrypt (large, 1MB) | < 50ms |
| Key generation | < 100ms |
| IPC round-trip | < 1ms |
| PD RAM | < 4MB (keys + crypto buffers) |

## Implementation Notes

- **Language:** C (bare metal — no libc in seL4 PDs)
- **Framework:** Microkit
- **Crypto:** monocypher (XSalsa20-Poly1305, Ed25519)
- **Key storage:** PD's private memory (seL4 frames not mapped to Linux)

## File Structure

```
src/sel4-partitions/key-vault-pd/
├── include/
│   ├── keyvault.h        # Public interface
│   ├── crypto.h          # Crypto operations
│   ├── storage.h         # Key storage (in-memory)
│   ├── ipc.h             # IPC interface
│   └── auth.h            # Authentication
├── keyvault.c            # Main PD logic
├── crypto.c              # monocypher wrappers
├── storage.c             # Key storage management
├── ipc.c                 # IPC handling
├── auth.c                # Authentication checking
└── CMakeLists.txt
```

---

**Spec version:** 1.0
**Ready for implementation:** Yes (after seL4 boots in QEMU)
