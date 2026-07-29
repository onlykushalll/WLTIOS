# ADR-06: musl libc (not glibc)

**Date:** 2026-06-21
**Status:** Accepted

## Context

WLTIOS needs a C standard library for the userspace. Options:

1. **glibc** — The GNU C Library. Used by most Linux distributions. Heavy, feature-rich.
2. **musl** — Lightweight, standards-compliant C library. Used by Alpine Linux.
3. **Custom** — Write our own. (Not realistic for v1.)

## Decision

**Use musl libc for all WLTIOS userspace components.**

## Consequences

### Positive
- **Small footprint** — musl is ~500KB. glibc is ~5MB. Saves 4.5MB RAM.
- **Fast** — musl is designed for efficiency. Simpler code paths than glibc.
- **Standards-compliant** — musl follows POSIX strictly. Cleaner than glibc.
- **Static linking friendly** — musl is designed for static linking. Smaller binaries.
- **Alpine Linux uses it** — Proven in production. Millions of Docker containers.
- **seL4 projects use it** — musllibc is already in the seL4 ecosystem (sel4-build/projects/musllibc).

### Negative
- **Some software won't compile** — Software that relies on glibc extensions (GNUisms) needs patching.
- **Fewer locales** — musl has simpler locale support than glibc.
- **No NSS (Name Service Switch)** — glibc's NSS is absent. We implement DNS differently.
- **Binary compatibility** — Can't run binaries compiled against glibc without a compatibility layer.

## Impact on WLTIOS

| Component | Impact |
|-----------|--------|
| Custom userspace (WLT-*) | ✅ No impact — we write code to musl's API |
| Tor VM | ✅ Use Alpine Linux (musl-based) |
| Third-party software | ⚠️ May need patching or Alpine packages |
| GUI apps (browser, editor) | ✅ Use Alpine packages or build from source |

## Alternatives Considered

### Alternative A: glibc
- **Pros:** Maximum compatibility. Every Linux app works.
- **Cons:** 10x larger than musl. Slower. Bloated.
- **Rejected because:** WLTIOS targets <256MB RAM idle. glibc's 5MB footprint is wasteful.

### Alternative B: bionic (Android's libc)
- **Pros:** Designed for mobile (low RAM)
- **Cons:** Android-specific. Not standard. License complexity.
- **Rejected because:** Not appropriate for desktop OS.

### Alternative C: Custom libc
- **Pros:** Exactly what we need
- **Cons:** Years of work. Reinventing the wheel.
- **Rejected because:** musl already does what we need.

## References

- musl libc: https://musl.libc.org/
- Alpine Linux: https://alpinelinux.org/
- seL4 musllibc: https://github.com/seL4/musllibc
- glibc vs musl: https://www.etalabs.net/compare_libcs.html

---

**Decision made:** 2026-06-21
**Revisit if:** Critical software requires glibc (unlikely — most software works with musl)
