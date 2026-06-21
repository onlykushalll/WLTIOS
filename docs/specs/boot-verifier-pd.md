# Spec: Boot Verifier PD (seL4 Partition)

> **Status:** Draft (ready for implementation)
> **Phase:** 3 (Month 15)
> **Dependencies:** seL4, Microkit
> **ADR references:** ADR-02 (seL4 base), ADR-04 (Microkit)
> **Research:** docs/context/RESEARCH_SEL4_PARTITIONS.md

## Purpose

The Boot Verifier PD is an seL4 Protection Domain that verifies the integrity of the running system at boot time AND at runtime. It detects rootkits, kernel modifications, and tampering — even if Linux has been compromised.

## How It Works

```
1. At boot:
   → WLT-Boot verifies seL4 signature (before loading seL4)
   → seL4 starts Boot Verifier PD
   → Boot Verifier verifies Linux kernel signature
   → Boot Verifier takes hash snapshots of critical files
   → If all pass: boot continues
   → If any fail: boot aborts, drops to recovery

2. At runtime (periodic):
   → Boot Verifier re-hashes critical files
   → Compares against boot-time snapshots
   → If mismatch: rootkit detected!
   → Action: trigger kill switch, alert user, offer rollback
```

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Linux kernel image | Memory (loaded by seL4) | ELF binary |
| Critical file list | Config (compiled into PD) | File paths + expected hashes |
| Runtime hash requests | Timer (periodic) | Trigger |
| Rollback command | User (via WLT-Init IPC) | Trigger |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Boot decision | seL4 | Continue boot / abort |
| Tamper alerts | WLT-Init (IPC) | Alert + affected file |
| Rollback trigger | WLT-Update (IPC) | Trigger A/B rollback |
| Status | WLT-Settings | Verification status |

## What's Verified

| Component | How Often | Method |
|-----------|-----------|--------|
| seL4 kernel | At boot | Signature (done by WLT-Boot) |
| Linux kernel | At boot + runtime | SHA-256 hash |
| Linux kernel modules | At boot | Per-module SHA-256 |
| WLT-Boot binary | At boot | Self-verify (measured boot) |
| seL4 PD binaries | At boot | Per-PD SHA-256 |
| Critical config files | At boot + runtime (every 60s) | SHA-256 |

## Dependencies

- **seL4** (provides isolation — Linux cannot tamper with Boot Verifier)
- **Microkit** (framework)
- **TPM** (optional — for hardware-backed measurement)

## What Depends on This

- WLT-Boot (uses verification result to decide boot)
- WLT-Update (uses verification to trigger rollback)
- User (notified if tampering detected)

## Error Handling

| Error | Action |
|-------|--------|
| Linux kernel hash mismatch at boot | **ABORT BOOT** — drop to recovery |
| Config file hash mismatch at runtime | **Alert user**, offer rollback, trigger kill switch |
| TPM unavailable | Fall back to software hashing, log warning |
| Boot Verifier self-check fails | **CRITICAL** — refuse to boot, alert user |

## Tests

| Test | How |
|------|-----|
| Normal boot | Boot WLTIOS, verify all checks pass |
| Tampered kernel | Modify Linux kernel, boot, verify abort |
| Runtime tamper | Modify critical file while running, verify alert |
| Rollback on tamper | Trigger tamper, verify rollback offered |
| Self-check | Corrupt Boot Verifier, verify self-detection |

## Security

- **seL4 isolation** — Linux cannot modify Boot Verifier or its snapshots
- **Snapshots in PD memory** — Hash snapshots stored in Boot Verifier's private memory
- **Periodic re-verification** — Catches runtime rootkits (not just boot-time)
- **Fail-closed** — Any verification failure → abort/alert
- **TPM integration** (optional) — Hardware root of trust

## Performance

| Metric | Budget |
|--------|--------|
| Boot verification | < 500ms |
| Runtime re-verification | < 100ms (every 60s) |
| PD RAM | < 2MB (snapshots + hashes) |

## File Structure

```
src/sel4-partitions/boot-verifier-pd/
├── include/
│   ├── verifier.h        # Public interface
│   ├── hashes.h          # Hash snapshot storage
│   ├── files.h           # Critical file list
│   └── ipc.h             # IPC interface
├── verifier.c            # Main PD logic
├── hashes.c              # SHA-256 computation + comparison
├── files.c               # File access (via seL4, not Linux)
├── ipc.c                 # IPC (alerts, rollback)
├── tpm.c                 # TPM integration (optional)
└── CMakeLists.txt
```

---

**Spec version:** 1.0
