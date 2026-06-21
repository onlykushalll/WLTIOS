# ADR-04: Microkit + libvmm (not CAmkES)

**Date:** 2026-06-21
**Status:** Accepted
**Research:** docs/context/RESEARCH_SEL4_PARTITIONS.md

## Context

WLTIOS needs a framework to build seL4 partitions (Protection Domains) and run Linux as a guest VM. Two options exist:

1. **CAmkES** — The older component framework for seL4. Has VM support (camkes-vm-examples).
2. **Microkit + libvmm** — The newer, recommended framework. seL4 Foundation's official recommendation.

## Decision

**Use Microkit + libvmm. Do not use CAmkES.**

## Consequences

### Positive
- **Officially recommended** — seL4 Foundation's whitepaper states: "Microkit is the recommended framework for building systems on seL4"
- **Simpler** — Microkit has a cleaner API than CAmkES
- **Active development** — Microkit is the future; CAmkES is being deprecated for VM work
- **libvmm** — Provides VMM (Virtual Machine Monitor) for running Linux guest
- **Better documentation** — Microkit manual is current and maintained
- **Community support** — seL4 Summit 2024 talks emphasize Microkit

### Negative
- **libvmm x86-64 is experimental** — The x86-64 VM support in libvmm is on an experimental SDK build. This is the #1 practical risk.
- **Fewer examples** — CAmkES has more VM examples (camkes-vm-examples). Microkit has fewer.
- **Learning curve** — Less community knowledge to draw on for Microkit + VM

### Risk Mitigation
- **De-risk first:** Boot libvmm `simple` example on QEMU x86_64 before committing. If it works, proceed. If not, fall back to CAmkES.
- **Fallback plan:** If libvmm x86-64 proves unusable, switch to CAmkES VMM (camkes-vm-examples minimal_64). The architecture doesn't change, only the framework.

## Alternatives Considered

### Alternative A: CAmkES
- **Pros:** More VM examples, more mature, camkes-vm-examples works today
- **Cons:** Being deprecated for VM work. seL4 Foundation recommends Microkit.
- **Rejected because:** Investing in a deprecated framework is wasteful. Microkit is the future.

### Alternative B: Raw seL4 (no framework)
- **Pros:** Maximum control
- **Cons:** Must implement everything ourselves. Months of extra work.
- **Rejected because:** Frameworks exist for a reason. Don't reinvent.

## References

- seL4 whitepaper (Microkit recommendation)
- Microkit manual: https://trustworthy.systems/projects/microkit/manual/
- libvmm: https://github.com/seL4/libvmm
- CAmkES deprecation: seL4 Summit 2024 talks
- Research: docs/context/RESEARCH_SEL4_PARTITIONS.md

---

**Decision made:** 2026-06-21
**Revisit if:** libvmm x86-64 proves unusable (fall back to CAmkES)
