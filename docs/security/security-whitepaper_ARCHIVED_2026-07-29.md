# WLTIOS Security Whitepaper

## Executive Summary

WLTIOS is a privacy-first desktop operating system that uses seL4 microkernel isolation to provide mathematical security guarantees unavailable in traditional monolithic-kernel OSes. This document describes the security architecture, threat model, and guarantees.

## Security Architecture

### seL4 Microkernel Foundation

seL4 is the only formally verified microkernel in the world. Its key properties:

1. **Functional correctness**: The implementation provably matches its specification
2. **Proof of no code execution**: The kernel cannot execute code paths not in its specification
3. **Proof of no buffer overflows**: The kernel cannot read/write outside intended memory
4. **Proof of no undefined behavior**: The kernel has no undefined behavior

These proofs are machine-checked (Isabelle/HOL) and cover ~10,000 lines of C code.

### Isolation Architecture

```
seL4 (verified, 10K lines)
├── Linux Guest VM (30M lines — untrusted)
│   ├── Apps (sandboxed via cgroups + namespaces)
│   ├── GUI (Wayland compositor)
│   └── Network (routed through Tor VM)
├── Tor VM (isolated — Tor daemon can't be touched by Linux)
├── Firewall PD (enforces Tor-only networking)
├── Key Vault PD (encryption keys never leave this partition)
└── Boot Verifier PD (detects tampering)
```

### Why seL4 Isolation Matters

In Tails, if the Linux kernel is compromised, everything is compromised — Tor, encryption keys, the works. In WLTIOS, even if the Linux kernel is fully owned:

- **Tor daemon is safe** — it runs in a separate VM, isolated by seL4
- **Encryption keys are safe** — they live in the Key Vault PD, isolated by seL4
- **Firewall is safe** — the kill switch runs in a PD, can't be disabled by Linux
- **Boot integrity is safe** — the Boot Verifier PD can detect tampering

seL4's formal proofs guarantee that Linux cannot break out of its VM, read PD memory, or interfere with PD execution.

## Threat Model

### Threats We Protect Against

| Threat | Mitigation | Guarantee |
|--------|-----------|-----------|
| Kernel exploit (Linux) | seL4 VM isolation | Mathematical (seL4 proof) |
| Tor compromise | Tor in separate VM | seL4 isolation |
| Key theft | Keys in Key Vault PD | seL4 isolation |
| Network leak (Tor down) | Kill switch in Firewall PD | seL4 isolation |
| Boot tampering (evil maid) | Boot Verifier PD + TPM | Measured boot |
| Disk theft | LUKS encryption (key in PD) | AES-256 |
| Traffic analysis | Tor (3-hop onion) | Tor design |
| DNS leak | DNS-over-Tor | Firewall enforcement |
| MAC tracking | MAC randomization on boot | Per-boot random |
| Forensic recovery | Amnesic mode (tmpfs) | No disk writes |

### Threats We Don't Fully Protect Against

| Threat | Status | Mitigation |
|--------|--------|-----------|
| Hardware compromise (implant) | ❌ Out of scope | Use hardware you trust |
| Side-channel attacks (Spectre) | ⚠️ Partial | seL4 has some protections |
| Physical seizure while running | ⚠️ Partial | Screen lock + memory encryption |
| $5 wrench attack | ❌ Out of scope | Deniability features (planned) |
| Global adversary (NSA) | ⚠️ Partial | Tor provides good but not perfect anonymity |

## Key Management

### Key Vault PD

All encryption keys live in the Key Vault PD — an seL4 Protection Domain with:
- No filesystem access (Microkit PD, not Linux process)
- No network access (isolated by seL4)
- No external IPC (only seL4 message-passing to authorized callers)
- Volatile memory only (keys generated at boot, not stored on disk)

### Key Persistence (TPM-Sealed Bootstrap)

The LUKS master key persistence uses a TPM-sealed approach:
1. At install: key generated, sealed to TPM PCR values, stored on ESP
2. At boot: WLT-Boot unseals key (TPM verifies firmware not tampered), passes to seL4
3. seL4 maps key into Key Vault PD memory BEFORE Linux starts
4. Key never appears in Linux kernel memory (except briefly for dm-crypt setup)

See `docs/specs/key-vault-pd.md` section "Key Persistence Architecture" for details.

## Boot Security

### Verified Boot Chain

```
UEFI → WLT-Boot (checks seL4 signature) → seL4 (starts PDs) → Boot Verifier PD
       (hashes Linux kernel + PDs) → Linux boots
```

Each stage verifies the next. If any verification fails, boot aborts to recovery.

### Runtime Integrity

The Boot Verifier PD periodically re-hashes in-memory binaries (not files — a Microkit PD has no filesystem). If a hash changes (rootkit injected into kernel memory), the PD triggers:
1. Kill switch (all network stops)
2. User alert
3. Rollback offer (via WLT-Update A/B partitions)

## Network Security

### Tor-Only Enforcement

The Firewall PD enforces that ALL network traffic goes through Tor:
- iptables rules in the Firewall PD (not Linux — Linux can't modify them)
- DNS routed through Tor (no DNS leaks)
- Kill switch: if Tor disconnects, ALL traffic stops instantly

### MAC Randomization

On every boot, MAC addresses are randomized:
- Prevents tracking across sessions
- Prevents correlation between user sessions
- Uses cryptographically secure random (not just time-seeded)

## App Sandboxing

Every app runs in a sandbox with:
- **cgroups v2**: CPU, memory, and I/O limits per app
- **namespaces**: Mount, PID, network, IPC isolation
- **seccomp-BPF**: Syscall filtering (apps can only call approved syscalls)
- **Permission manifest**: Each app declares what it needs (network, files, camera)

### Privacy Badges

Apps are rated based on their permissions:
- 🟢 Green: No network, sandboxed files
- 🟡 Yellow: Tor-only network, limited file access
- 🔴 Red: Direct network or broad file access

## Kairos AI Security

### Local-First Design

Kairos defaults to a LOCAL model (VibeThinker-3B, 3B parameters, Q4 GGUF ~2GB). No data leaves the machine.

### Cloud API Privacy Pipeline

When a user adds a cloud API key (Gemini, Claude, etc.):
1. Request is analyzed by the privacy filter (strips PII, file paths, credentials)
2. Request is routed through Tor (IP anonymized)
3. Response is received through Tor
4. Response is sanitized before being shown to the user

This means even with a cloud API, the cloud provider cannot:
- See the user's real IP (Tor)
- Receive file paths or system information (privacy filter)
- Correlate requests across sessions (Tor + no persistent identifiers)

### Guardian Module

Kairos includes an active defense guardian that:
- Monitors network connections for suspicious activity
- Detects and blocks port scanning attempts
- Warns about suspicious process behavior
- Can freeze network activity if an attack is detected

## Conclusion

WLTIOS provides security guarantees that are mathematically proven (seL4), not just empirically tested. The isolation architecture ensures that even a full Linux kernel compromise cannot expose encryption keys, disable the firewall, or compromise Tor. This is a level of security unavailable in any mainstream desktop OS.
