# ADR-04: Microkit + libvmm (not CAmkES)

**Date:** 2026-06-21
**Status:** Accepted (with amendment)
**Research:** docs/context/RESEARCH_SEL4_PARTITIONS.md

## Context

WLTIOS needs a framework to build seL4 partitions (Protection Domains) and run Linux as a guest VM.

## Decision

**Use Microkit + libvmm for production. Use CAmkES for current development.**

## Amendment (2026-06-22)

The working seL4 build (that boots Linux to a login prompt in QEMU) uses **CAmkES** (camkes-vm-examples), NOT Microkit. This is because:

1. Microkit's libvmm x86-64 VM support is experimental
2. CAmkES has working x86-64 VM examples (minimal_64, minimal, optiplex9020)
3. The seL4 Foundation recommends Microkit for new projects, but CAmkES is the proven path for x86-64 VMs today

**Transition plan:** When Microkit libvmm x86-64 matures, migrate from CAmkES to Microkit. The architecture doesn't change — only the framework.

## Consequences

### Positive
- **CAmkES works NOW** — proven with our QEMU boot test
- **Microkit is the future** — seL4 Foundation's recommended framework
- **Architecture is framework-agnostic** — our seL4 partitions work with either

### Negative
- **Two frameworks** — code may need migration when switching
- **Microkit x86-64 VM is experimental** — risk of bugs

## References

- seL4 CAmkES VM examples: https://github.com/seL4/camkes-vm-examples
- Microkit manual: https://trustworthy.systems/projects/microkit/manual/
- Working build: `~/wltios-build/build-minimal-32/` (CAmkES, boots to login)

---

**Decision made:** 2026-06-21
**Amended:** 2026-06-22 (document actual CAmkES usage)
