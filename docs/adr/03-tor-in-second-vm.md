# ADR-03: Tor in Second Linux VM (not native seL4 partition)

**Date:** 2026-06-21
**Status:** Accepted (supersedes original architecture)
**Research:** docs/context/RESEARCH_SEL4_PARTITIONS.md

## Context

The original WLTIOS architecture planned to run Tor directly in a seL4 partition. Research revealed this is **not feasible** for v1.

### The Problem

Tor is a ~400,000-line POSIX application that requires:
- A C library (glibc or musl)
- Socket APIs (TCP/UDP networking)
- Filesystem access (for state, circuits, keys)
- Multiple threads
- DNS resolution

seL4 partitions (Protection Domains in Microkit) provide:
- No libc (must write bare-metal C)
- No sockets (must implement network stack from scratch)
- No filesystem (must implement or use RAM-disk)
- No threads (single-threaded by default)
- No DNS (must implement)

Porting Tor to run natively in a seL4 partition would require:
- Porting musl libc to seL4 (6-12 months)
- Implementing a TCP/IP stack in the partition (3-6 months)
- Implementing a filesystem layer (2-4 months)
- Adapting Tor's threading model (2-3 months)

**Total: 13-25 months just to run Tor natively.** This exceeds our entire Phase 1 budget.

## Decision

**Run Tor in a SECOND Linux guest VM, isolated by seL4 from the main Linux guest.**

### Updated Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    WLTIOS ARCHITECTURE (UPDATED)           │
├─────────────────────────────────────────────────────────┤
│  ┌─────────────────────┐  ┌─────────────────────────┐  │
│  │  MAIN LINUX GUEST   │  │  TOR LINUX GUEST         │  │
│  │  (GUI, apps,        │  │  (Tor daemon only)       │  │
│  │   user workspace)   │  │                          │  │
│  │                     │  │  - Minimal Linux kernel  │  │
│  │  - Browser          │  │  - Tor + dependencies    │  │
│  │  - Editor           │  │  - No GUI, no user apps  │  │
│  │  - File manager     │  │  - Network: ONLY to NIC  │  │
│  │  - AI assistant     │  │    (via Firewall PD)     │  │
│  │                     │  │                          │  │
│  │  Network: via Tor   │  │  Identity keys: stored   │  │
│  │  VM (not direct)    │  │  in Key Vault PD         │  │
│  └──────────┬──────────┘  └───────────┬─────────────┘  │
│             │                          │                  │
│  ┌──────────▼──────────────────────────▼─────────────┐  │
│  │              seL4 MICROKERNEL                        │  │
│  │                                                      │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌────────────┐ │  │
│  │  │ FIREWALL PD │  │ KEY VAULT PD │  │ BOOT VERIF │ │  │
│  │  │             │  │              │  │    PD      │ │  │
│  │  │ - All traffic│  │ - Encryption │  │ - Runtime  │ │  │
│  │  │   passes    │  │   keys       │  │   integrity│ │  │
│  │  │   through   │  │ - Tor identity│ │   check    │ │  │
│  │  │   this      │  │   keys       │  │            │ │  │
│  │  │ - Kill switch│ │ - Crypto ops │  │            │ │  │
│  │  └─────────────┘  └──────────────┘  └────────────┘ │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## Consequences

### Positive
- **Tor works without porting** — Runs in standard Linux environment
- **Stronger than Tails** — Tor VM is isolated from main Linux by seL4. If main Linux is compromised, attacker cannot touch Tor.
- **Identity keys protected** — Tor's identity keys stored in Key Vault PD (seL4 partition). Even if Tor VM is compromised, keys are safe.
- **Firewall enforced** — All network traffic (from BOTH Linux VMs) passes through Firewall PD. No bypass possible.
- **Kill switch is trivial** — Firewall PD drops all packets on command. Both VMs lose network instantly.
- **Feasible in Phase 1** — No 13-25 month porting effort

### Negative
- **Two Linux VMs** — More memory usage (~512MB for Tor VM)
- **More complex networking** — Main Linux → Tor VM → NIC (extra hop)
- **Not "pure" seL4** — Tor still runs in Linux, not bare seL4. (But it's isolated, which is what matters.)

### Security Comparison

| Threat | Tails | Original WLTIOS Plan | Updated WLTIOS Plan |
|--------|-------|---------------------|---------------------|
| Linux kernel hacked | Tor compromised | Tor safe (seL4 partition) | Tor safe (separate VM) |
| Tor process hacked | Full system compromise | Tor safe (no Linux access) | Tor VM compromised, but keys safe (in PD) |
| Network leak | Possible if firewall bypassed | Impossible (PD enforces) | Impossible (PD enforces) |
| Key theft | Possible (keys in /etc) | Impossible (keys in PD) | Impossible (keys in PD) |

## Alternatives Considered

### Alternative A: Native seL4 Tor (original plan)
- **Pros:** Maximum isolation. Tor runs without Linux.
- **Cons:** 13-25 months porting effort. Exceeds Phase 1 budget.
- **Rejected because:** Not feasible in timeline.

### Alternative B: Tor in main Linux guest (like Tails)
- **Pros:** Simplest. One VM.
- **Cons:** If Linux is hacked, Tor is compromised. No better than Tails.
- **Rejected because:** WLTIOS's value is stronger security than Tails.

### Alternative C: Tor in seL4 partition with minimal libc
- **Pros:** Better isolation than VM
- **Cons:** Still needs TCP/IP stack, filesystem. 6-12 months minimum.
- **Rejected because:** Still too much work for v1.

## Implementation Notes

- Tor VM: Minimal Linux (Buildroot-based), ~50MB rootfs, only Tor + dependencies
- Communication: Main Linux → virtio-net → Firewall PD → Tor VM → NIC
- Tor identity keys: Generated in Key Vault PD, passed to Tor VM via secure channel
- Tor VM has NO persistent storage — state is ephemeral (amnesic)
- If Tor VM crashes: Firewall PD blocks all traffic (kill switch)

## References

- Research: docs/context/RESEARCH_SEL4_PARTITIONS.md (section: "Can Tor run in seL4?")
- Microkit manual: https://trustworthy.systems/projects/microkit/manual/
- libvmm: https://github.com/seL4/libvmm
- Tor design: https://2019.www.torproject.org/docs/overview

---

**Decision made:** 2026-06-21
**Decision maker:** Kushal + orchestrator (based on research findings)
**Revisit if:** A native seL4 Tor port becomes available, or if Tor VM proves too resource-heavy
