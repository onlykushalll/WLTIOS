# ADR-01: UEFI Boot (not BIOS)

**Date:** 2026-06-21
**Status:** Accepted
**Research:** docs/context/RESEARCH_SEL4_LINUX_VM.md

## Context

WLTIOS needs a boot mechanism. The two options are:

1. **UEFI (Unified Extensible Firmware Interface)** — Modern firmware, replaced BIOS in 2010s
2. **BIOS (Basic Input/Output System)** — Legacy firmware from the 1980s

## Decision

**Use UEFI exclusively. Do not support BIOS boot.**

## Consequences

### Positive
- **Modern hardware support** — All PCs since ~2012 ship with UEFI. BIOS is legacy.
- **Secure Boot** — UEFI supports signature verification (critical for our verified boot)
- **GPT partitioning** — UEFI uses GPT (supports >2TB disks, >4 partitions). BIOS uses MBR (limited).
- **Faster boot** — UEFI initializes hardware faster than BIOS
- **seL4 support** — seL4's x86-64 port targets UEFI, not BIOS
- **Large disk support** — GPT supports disks > 2TB
- **Network boot** — UEFI has native HTTP boot (PXE is legacy)

### Negative
- **Older hardware unsupported** — PCs before 2012 can't run WLTIOS. This is acceptable.
- **CSM (Compatibility Support Module) not supported** — We won't boot via BIOS emulation.

## Alternatives Considered

### Alternative A: BIOS Boot
- **Pros:** Works on older hardware (pre-2012)
- **Cons:** No Secure Boot, MBR only (2TB limit), slower, seL4 doesn't target it
- **Rejected because:** Modern hardware is UEFI. Supporting BIOS adds complexity for <5% of users.

### Alternative B: Support Both UEFI and BIOS
- **Pros:** Maximum hardware compatibility
- **Cons:** Doubles bootloader complexity. Two code paths. Two partition schemes.
- **Rejected because:** BIOS is dead technology. Not worth the maintenance burden.

## References

- UEFI Specification: https://uefi.org/specifications
- seL4 x86-64 boot requirements: docs.sel4.systems
- GPT vs MBR: UEFI spec section 5

---

**Decision made:** 2026-06-21
**Revisit if:** A significant user base has pre-2012 hardware (unlikely)
