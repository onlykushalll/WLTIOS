# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| latest main branch | ✅ |
| tagged releases | ✅ |
| development branches | ❌ |

## Reporting a Vulnerability

**Do NOT open a public GitHub issue for security vulnerabilities.**

Instead, please report vulnerabilities responsibly:

1. **Email:** (security contact to be set up — for now, use GitHub's private vulnerability reporting)
2. **GitHub:** Use the "Security" tab → "Report a vulnerability" button
3. **Response time:** Within 48 hours

Please include:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

## Security Architecture

WLTIOS uses seL4 microkernel isolation to provide mathematical security guarantees. See:
- [Security Whitepaper](security/security-whitepaper.md) — full threat model and architecture
- [Audit Results](security/audit-results.md) — static analysis results
- [ADR-02](adr/02-sel4-as-base.md) — seL4 as base architecture decision

### Key Security Properties

1. **seL4 formal verification** — The kernel is mathematically proven to have no bugs, no buffer overflows, no undefined behavior
2. **Isolation** — Linux compromise cannot touch: Tor daemon, encryption keys, firewall, boot verifier
3. **Tor-only networking** — Enforced by seL4 Firewall PD, not Linux iptables
4. **Kill switch** — If Tor dies, ALL network stops (enforced by PD)
5. **Amnesic mode** — No disk writes in Live USB mode (tmpfs)
6. **Disk encryption** — LUKS2 AES-256, key in Key Vault PD
7. **MAC randomization** — New MAC every boot
8. **Verified boot** — Signature checks at each boot stage

### What seL4 Does NOT Protect Against

- Hardware implants (use hardware you trust)
- Side-channel attacks (Spectre/Meltdown — seL4 has partial protections)
- Physical seizure while running (screen lock helps)
- $5 wrench attack (deniability features planned)

## Security Audit

A static security audit was performed on 2026-06-22. Results:
- 45 source files audited
- 15 issues found and fixed
- 0 buffer overflow vulnerabilities
- 0 format string vulnerabilities
- 0 hardcoded credentials
- Report: [docs/security/audit-results.md](security/audit-results.md)

Dynamic penetration testing (8-test suite) is in `tests/integration/pentest.sh`.

## Dependency Security

| Dependency | Version | Security Notes |
|-----------|---------|----------------|
| seL4 | latest | Formally verified (Isabelle/HOL proofs) |
| Linux | 4.8.16 (guest) | Minimal config, runs as untrusted VM |
| wlroots | 0.18.2 | Built with -Dwerror=false (GCC 15 compat) |
| musl | 1.2.x | Lightweight, clean libc |
| monocypher | latest | Used in Key Vault PD (bare-metal crypto) |
| Tor | latest | Runs in isolated VM |

## Responsible Disclosure

We follow responsible disclosure. If you report a vulnerability:
1. We will acknowledge receipt within 48 hours
2. We will investigate and confirm within 7 days
3. We will fix and release a patched version within 30 days
4. We will credit you in the release notes (unless you prefer to remain anonymous)

## License

This security policy is part of the WLTIOS project, licensed under GPL-3.0.
