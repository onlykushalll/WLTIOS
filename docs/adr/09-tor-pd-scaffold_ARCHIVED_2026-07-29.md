# ADR-09: TorPD as Second Linux Guest VM (Phase 1: IPC Scaffold)

**Date:** 2026-07-19
**Status:** Accepted (Phase 1: spec + minimal IPC proof)
**Supersedes:** None (implements ADR-03's decision)
**Spec:** docs/specs/wlt-tor-pd.md

## Context

ADR-03 (2026-06-21) decided that Tor runs in a **second Linux guest VM**,
isolated by seL4 from the main Linux guest, rather than as a native seL4
partition (which would require 13-25 months of porting work). This ADR
covers the Phase 1 implementation decision: how to scaffold the TorPD
component and prove the isolation boundary works.

The Key Vault PD (verified 2026-07-12) proved the CAmkES multi-PD pattern:
- A bare seL4 component (`KeyVaultPD`) runs alongside the Linux VM
- They communicate via shared memory (`seL4SharedDataWithCaps`) + notifications (`seL4Notification`)
- The Linux guest accesses shared memory via PCI-based cross-VM connections
- Ping/pong round-trip verified in QEMU serial log

## Decision

**Phase 1: Scaffold TorPD as a CAmkES component that mirrors KeyVaultPD's
ping/pong IPC pattern, but is declared as a second Linux guest VM (vm1)
rather than a bare seL4 component.**

### Why a second Linux VM (not a bare seL4 component)?

Tor requires:
- glibc/musl (POSIX C library)
- TCP/UDP sockets (real network stack)
- Filesystem (for state, circuits, keys)
- Multiple threads

A bare seL4 component has none of these. KeyVaultPD works as a bare component
because it only does crypto operations (no I/O). TorPD needs a full Linux
environment, so it must be a **guest VM** (like vm0), not a bare component.

### Why not just run Tor in vm0?

That's what Tails does. If vm0's kernel is compromised, Tor is compromised.
WLTIOS's value proposition is that Tor is isolated in a **separate** VM —
if vm0 is hacked, the attacker cannot touch vm1 (Tor).

### The IPC pattern (same as KeyVaultPD)

```
vm0 (main Linux)                          vm1 (TorPD Linux)
  │                                          │
  │  writes "ping" to shared memory         │
  │  emits ready notification ──────────────►│
  │                                          │  ready_wait() returns
  │                                          │  reads "ping" from shared mem
  │                                          │  writes "pong" to shared mem
  │  ◄──────────────── done notification     │  emits done notification
  │  reads "pong" from shared memory         │
```

This is the exact pattern KeyVaultPD uses, extended to VM-to-VM (instead of
VM-to-component).

## Consequences

### Positive
- **Proves the isolation boundary works** — ping/pong across the seL4 kernel
  boundary between two Linux guests is the foundational proof that TorPD can
  be isolated from the main guest.
- **Reuses the KeyVaultPD pattern** — no new CAmkES patterns invented, just
  applied to a second VM instead of a bare component.
- **Incremental** — Phase 1 is just IPC proof. Phase 2 (Tor actually running
  in vm1) can build on this scaffold without redesigning the isolation layer.

### Negative
- **Two Linux VMs** — more memory (~512MB for vm1 in addition to vm0's 512MB).
  QEMU may need `-m 2G` instead of `-m 1G`.
- **Build complexity** — the CAmkES assembly must declare two VM components
  (vm0 + vm1) plus KeyVaultPD, with separate kernel images and initramfs.
- **Network routing not solved yet** — Phase 1 proves IPC, not Tor traffic
  routing. The actual "all traffic goes through TorPD" enforcement is Phase 2+
  (needs Firewall PD).

### Risk: VM-to-VM IPC vs VM-to-component IPC

KeyVaultPD does VM-to-component IPC (vm0 ↔ bare KeyVaultPD component).
TorPD needs VM-to-VM IPC (vm0 ↔ vm1). The cross_vm_connections.c pattern
should work for both, but this hasn't been proven yet in this project.
**If VM-to-VM IPC doesn't work with the current CAmkES setup, Phase 1 will
fail and the design must be revisited.**

## Alternatives Considered

### Alternative A: TorPD as bare seL4 component (no Linux)
- **Pros:** Maximum isolation. No Linux kernel in TorPD.
- **Cons:** Tor needs libc/sockets/fs/threads — 13-25 months to port (ADR-03).
- **Rejected:** Not feasible in timeline.

### Alternative B: TorPD as bare seL4 component with minimal SOCKS proxy
- **Pros:** No second Linux VM (less memory).
- **Cons:** Must implement a SOCKS proxy + TCP/IP stack in bare seL4 — 6-12
  months minimum. The proxy would need to handle streaming, connection
  multiplexing, DNS — all from scratch.
- **Rejected:** Still too much work for v1.

### Alternative C: TorPD in a container (not a separate VM)
- **Pros:** Simpler. One Linux kernel.
- **Cons:** No seL4 isolation. If the kernel is compromised, the container
  is too. No better than Tails.
- **Rejected:** Defeats the purpose.

## Implementation Notes

- The TorPD CAmkES component (`tor_pd.c`) is a copy of `key_vault_pd.c` with
  the serial prefix changed from `[key-vault-pd]` to `[tor-pd]`.
- The app assembly (`minimal_tor_pd.camkes`) is based on `minimal_keyvault.camkes`
  but adds a second VM component (vm1) with its own kernel + initramfs.
- The build uses the same CAmkES VM helpers (`DeclareCAmkESVM`) as keyvault_pd.
- Phase 1 does NOT run Tor inside vm1 — vm1 is a minimal Linux that just
  runs the ping/pong test. Tor integration is Phase 2.

## Verification

Phase 1 is "done" when the QEMU serial log shows:
```
[tor-pd] alive
[tor-pd] Waiting for ping from Linux VM...
[tor-pd] ping received, pong sent
Result from Tor PD: pong
```

This mirrors the Key Vault PD verification bar from 2026-07-12.

---

**Decision made:** 2026-07-19
**Phase:** 1 (IPC scaffold) — Phase 2 (Tor in vm1) is follow-up work
