# Spec: WLT-Crypto (Encryption Layer)

> **Status:** Draft (ready for implementation)
> **Phase:** 2 (Month 11)
> **Dependencies:** Key Vault PD, musl libc
> **ADR references:** ADR-06 (musl libc)

## Purpose

WLT-Crypto handles all disk encryption, file encryption, and cryptographic operations. It does NOT store keys — keys live in the seL4 Key Vault PD. WLT-Crypto is the bridge between apps that need crypto and the Key Vault that holds the keys.

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Encrypt/decrypt requests | Apps (via IPC) | Plaintext/ciphertext + key ID |
| Key operations | Key Vault PD (IPC) | Key handles (keys never leave PD) |
| Disk config | `/etc/wlt/crypto.conf` | INI-style text |
| LUKS headers | Disk partitions | LUKS2 metadata |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Encrypted data | Apps, disk | Ciphertext |
| Decrypted data | Apps | Plaintext |
| LUKS-encrypted partitions | Disk | Encrypted block device |
| Status | WLT-Settings | Encryption status |

## Dependencies

- **Key Vault PD** (seL4 partition — stores all keys, performs crypto operations)
- **Linux dm-crypt** (kernel module for transparent disk encryption)
- **LUKS2** (Linux Unified Key Setup — disk encryption standard)
- **libsodium** (userspace crypto for file encryption)

## What Depends on This

- WLT-Init (mounts encrypted /users partition)
- WLT-FileManager (per-file encryption)
- WLT-Pkg (package signature verification)
- WLT-Update (update signature verification)
- All apps that need encryption

## Error Handling

| Error | Action |
|-------|--------|
| Key Vault unreachable | Refuse all crypto operations. Log critical error. |
| Decryption failed (wrong key) | Return error, log attempt (possible attack) |
| Disk encryption setup failed | Abort boot, drop to recovery shell |
| LUKS header corrupt | Attempt recovery, warn user |
| Crypto operation timeout | Retry 3 times, then fail |

## Tests

| Test | How |
|------|-----|
| Disk encryption | Install WLTIOS, verify disk is encrypted at rest |
| File encryption | Encrypt file, verify can't read without key |
| File decryption | Decrypt file, verify content matches original |
| Key Vault integration | Encrypt with key from PD, verify key never leaves PD |
| LUKS unlock | Boot, enter password, verify partition mounts |
| Wrong password | Enter wrong password, verify refusal |
| Corrupt header | Corrupt LUKS header, verify detection + recovery |

## Security

- **Keys never leave Key Vault PD** — All crypto operations happen INSIDE the seL4 partition. WLT-Crypto sends plaintext/ciphertext via IPC; the PD does the actual encryption/decryption.
- **No key caching** — WLT-Crypto never stores keys in memory. Each operation requests the PD.
- **Secure delete** — When deleting encrypted files, overwrite with random data 3 times before unlink.
- **Memory zeroing** — All plaintext buffers zeroed after use (sodium_memzero).

## Performance

| Metric | Budget |
|--------|--------|
| Disk encrypt/decrypt throughput | > 500 MB/s (AES-NI hardware) |
| File encrypt latency | < 10ms per file |
| Key Vault IPC round-trip | < 1ms |
| LUKS unlock | < 500ms |
| WLT-Crypto RAM | < 15MB |

## File Structure

```
src/userspace/wlt-crypto/
├── include/
│   ├── crypto.h          # Public crypto API
│   ├── keyvault.h        # Key Vault IPC
│   ├── luks.h            # LUKS disk encryption
│   ├── file_crypto.h     # Per-file encryption
│   └── secure_delete.h   # Secure file deletion
├── crypto.c              # Main crypto interface
├── keyvault.c            # IPC to Key Vault PD
├── luks.c                # LUKS2 setup + unlock
├── file_crypto.c         # File-level encryption (libsodium)
├── secure_delete.c       # shred implementation
├── dm_crypt.c            # dm-crypt device mapper setup
├── config.c              # Config parsing
└── Makefile
```

## API

```c
// Encrypt a file (key stays in Key Vault PD)
wlt_Error wlt_crypto_encrypt_file(const char *path, const char *key_id);

// Decrypt a file
wlt_Error wlt_crypto_decrypt_file(const char *path, const char *key_id);

// Encrypt a buffer (for in-memory data)
wlt_Error wlt_crypto_encrypt(const uint8_t *plaintext, size_t len,
                              const char *key_id,
                              uint8_t *ciphertext, size_t *out_len);

// Decrypt a buffer
wlt_Error wlt_crypto_decrypt(const uint8_t *ciphertext, size_t len,
                              const char *key_id,
                              uint8_t *plaintext, size_t *out_len);

// Disk encryption
wlt_Error wlt_crypto_luks_format(const char *device, const char *key_id);
wlt_Error wlt_crypto_luks_open(const char *device, const char *name,
                                const char *key_id);
wlt_Error wlt_crypto_luks_close(const char *name);

// Secure delete
wlt_Error wlt_crypto_shred(const char *path, int passes);
```

## Disk Encryption Layout

```
/dev/sda1 (ESP, unencrypted)     — Bootloader + seL4 + Linux kernel
/dev/sda2 (LUKS2 encrypted)      — Root filesystem (system/)
/dev/sda3 (LUKS2 encrypted)      — User data (/users/)
/dev/sda4 (LUKS2 encrypted)      — Persistent storage (if enabled)
```

---

**Spec version:** 1.0
