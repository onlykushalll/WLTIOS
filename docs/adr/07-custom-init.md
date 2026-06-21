# ADR-07: Custom Init System (not systemd)

**Date:** 2026-06-21
**Status:** Accepted

## Context

WLTIOS needs an init system (PID 1 — the first userspace process). Options:

1. **systemd** — Used by most Linux distributions. ~300K lines of code. Heavy.
2. **OpenRC** — Lighter than systemd. Used by Gentoo, Alpine.
3. **runit** — Very minimal. Used by Void Linux.
4. **Custom** — Write our own. Simple for our needs.

## Decision

**Write a custom init system (WLT-Init). Do not use systemd, OpenRC, or runit.**

## Rationale

WLTIOS is not a general-purpose Linux distribution. It's a focused OS with:
- A fixed set of services (no user-installed daemons)
- No need for systemd's complexity (dependency graphs, socket activation, etc.)
- A goal of <256MB RAM idle (systemd uses ~100MB+)
- A need for Tor-aware networking (systemd doesn't understand Tor)

A custom init system gives us:
- ~20K lines of code (vs systemd's 300K)
- ~5MB RAM usage (vs systemd's 100MB+)
- Full control over boot sequence
- Tor integration from PID 1

## Consequences

### Positive
- **Minimal** — 20K lines vs 300K. Easier to audit, understand, maintain.
- **Fast boot** — No dependency graph resolution. Sequential startup. <2 seconds.
- **Low RAM** — ~5MB vs 100MB+. Critical for our 256MB budget.
- **Tor-aware** — Init knows about Tor. Won't start network-dependent services until Tor is ready.
- **Kill switch integration** — If Tor dies, init can freeze network-dependent services.
- **No systemd complexity** — No journalctl, no systemctl, no dbus dependency.

### Negative
- **We maintain it** — No community support like systemd has.
- **Less features** — No socket activation, no cgroups management (we do that separately).
- **Learning curve** — Users familiar with `systemctl` need to learn our commands.

## WLT-Init Design

### Responsibilities
1. **Mount filesystems** — /, /proc, /sys, /tmp (tmpfs), /users (encrypted)
2. **Start essential services** — in order:
   - Check filesystem integrity
   - Mount persistent storage (if persistence enabled)
   - Start key vault communication (seL4)
   - Start Tor VM (via seL4)
   - Wait for Tor to be ready
   - Start network manager (routes through Tor)
   - Start WLT-Compositor (GUI)
   - Start WLT-Shell (taskbar, launcher)
   - Start WLT-AI (assistant)
   - Start user session
3. **Monitor services** — Restart on crash (with backoff)
4. **Handle shutdown** — Clean unmount, sync, power off

### Service Definition Format

```ini
# /etc/wlt/services/tor-vm.service
[wlt-service]
name=Tor VM
exec=/usr/bin/wlt-tor-vm-start
depends=key-vault
tor_required=yes
restart_on_crash=yes
restart_backoff_ms=5000
```

### Commands
```bash
wltctl status              # Show all services
wltctl start <service>     # Start a service
wltctl stop <service>      # Stop a service
wltctl restart <service>   # Restart a service
wltctl logs <service>      # Show service logs
wltctl enable <service>    # Enable at boot
wltctl disable <service>   # Disable at boot
```

## Alternatives Considered

### Alternative A: systemd
- **Pros:** Battle-tested. Every Linux admin knows it.
- **Cons:** 300K lines. 100MB+ RAM. Overkill for our needs.
- **Rejected because:** Too heavy for 256MB RAM budget.

### Alternative B: OpenRC
- **Pros:** Lighter than systemd. Proven (Gentoo, Alpine).
- **Cons:** Still more complex than we need. Not Tor-aware.
- **Rejected because:** We need Tor integration from PID 1.

### Alternative C: runit
- **Pros:** Very minimal. ~10K lines.
- **Cons:** Too minimal. No service dependency management.
- **Rejected because:** We need dependency management (Tor before network).

## References

- systemd: https://systemd.io/
- OpenRC: https://github.com/OpenRC/openrc
- runit: http://smarden.org/runit/
- Alpine Linux init: https://wiki.alpinelinux.org/wiki/Alpine_Init_System

---

**Decision made:** 2026-06-21
**Revisit if:** Maintaining custom init proves too burdensome (consider OpenRC fallback)
