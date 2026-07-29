# ADR-02: seL4 as Base with Linux as Guest VM

**Date:** 2026-06-21
**Status:** Accepted
**Research:** docs/context/RESEARCH_SEL4_LINUX_VM.md

## Context

WLTIOS needs a kernel foundation. The three options were:

1. **Pure Linux** (like Tails/Ubuntu) — proven, but no formal security guarantees
2. **seL4 + Linux guest** (like Qubes, but with seL4 instead of Xen) — formal verification for critical services, Linux for hardware support
3. **Pure seL4** (write everything from scratch) — maximum security, but 5-10 year timeline

The core requirement: critical services (Tor, encryption keys, firewall) must be isolated from the general-purpose OS (Linux) such that even a full Linux kernel compromise cannot touch them.

## Decision

**Use seL4 as the verified microkernel base, with Linux running as a guest VM.**

- seL4 provides formally verified isolation between partitions
- Linux runs as a guest VM, providing hardware drivers (GPU, WiFi, USB, storage)
- Critical services (Tor, key vault, firewall, boot verifier, auth) run in seL4 partitions, isolated from Linux
- If Linux is compromised, critical services remain secure

## Consequences

### Positive
- Mathematical security guarantees for critical services (seL4 is formally verified)
- Full hardware support via Linux drivers (no need to write GPU/WiFi drivers)
- Proven architecture — seL4 Foundation has reference VM examples
- Tor in seL4 partition = stronger anonymity than Tails (which runs Tor in Linux)
- Differentiator from Tails/Qubes (neither uses seL4)

### Negative
- seL4's VMM (hypervisor extensions) is NOT formally verified — only the seL4 kernel itself is
- GPU passthrough on seL4 has never been publicly demonstrated (biggest technical risk)
- seL4 is not designed for desktop use — we're pioneering this space
- Performance overhead: ~5-10% for VM isolation (acceptable)
- Complex development — seL4 has a steep learning curve

### Risk Mitigation
- **GPU passthrough risk:** De-risk FIRST by booting seL4 + Linux in QEMU, then attempting PCI passthrough of a network card. If that works, GPU is a configuration problem. If not, fall back to pure Linux + app-level sandboxing (like Tails).
- **VMM verification risk:** Accept that the VMM is not verified. The seL4 kernel IS verified, which means even an unverified VMM cannot break partition isolation.
- **Desktop readiness risk:** Start with seL4 Foundation's reference examples. Iterate.

## Alternatives Considered

### Alternative A: Pure Linux (like Tails)
- **Pros:** Proven, easy, full hardware support
- **Cons:** No formal security guarantees. If Linux kernel is hacked, everything is compromised.
- **Rejected because:** WLTIOS's unique value is security stronger than Tails. Pure Linux doesn't provide that.

### Alternative B: Pure seL4 (write everything from scratch)
- **Pros:** Maximum security, full control
- **Cons:** 5-10 year timeline. Must write all drivers. Not viable in 1-2 years.
- **Rejected because:** Timeline doesn't fit. We need Linux for hardware support.

### Alternative C: Xen hypervisor (like Qubes)
- **Pros:** Proven (Qubes uses it), good isolation
- **Cons:** Xen is NOT formally verified. seL4 IS. Using Xen would give us "Qubes but different" — not a meaningful improvement.
- **Rejected because:** seL4's formal verification is the differentiator. Without it, we're just another Qubes.

## References

- seL4 Foundation FAQ: https://sel4.systems/About/FAQ.html
- seL4 CAmkES VM examples: https://github.com/seL4/camkes-vm-examples
- Makatea project (Qubes-on-seL4 attempt): https://trustworthy.systems/projects/OLD/makatea
- Full research: docs/context/RESEARCH_SEL4_LINUX_VM.md

---

**Decision made:** 2026-06-21
**Decision maker:** Kushal (project owner) + WLTIOS orchestrator
**Revisit if:** GPU passthrough proves impossible on seL4 (would require architecture change)
