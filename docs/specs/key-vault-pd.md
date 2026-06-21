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
