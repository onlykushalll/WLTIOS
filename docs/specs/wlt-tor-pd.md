# Spec: WLT-Tor-PD (Tor Protection Domain)

> **Status:** Draft (Phase 1: spec + minimal IPC proof)
> **Phase:** 3 (Month 10+)
> **Dependencies:** seL4, CAmkES, Key Vault PD (template), ADR-03 (Tor in second VM)
> **ADR references:** ADR-03 (Tor in second VM), ADR-04 (Microkit/libvmm), ADR-09 (this spec's design decision)

## Purpose

Isolate the Tor daemon in a seL4-protected partition so that even if the main
Linux guest is compromised, the attacker cannot touch Tor's process, circuits,
or identity keys. This is WLTIOS's core differentiator over Tails ("the thing
that makes WLTIOS categorically better than Tails" — ADR-03).

## The Hard Part (stated plainly)

Key Vault PD was straightforward: it only needs **request/response IPC** (ping
→ pong, encrypt → ciphertext). No raw network I/O crosses the isolation
boundary.

Tor PD is fundamentally different: **Tor needs real external network I/O**
(TCP connections to Tor relays, DNS resolution, SOCKS proxy traffic from apps).
This means the isolation boundary must carry either:

1. **Raw packet flow** (the Tor PD has its own NIC and does real TCP), OR
2. **Proxy protocol** (apps send SOCKS5 requests via IPC to the Tor PD, which
   makes the real network connections on their behalf)

Option 1 is simpler conceptually but requires the Tor PD to have its own
network stack (a second Linux guest VM, per ADR-03). Option 2 would require
porting a SOCKS proxy into a bare seL4 component — infeasible (see ADR-03's
"13-25 months" estimate).

**Decision: Option 1 — Tor PD is a second Linux guest VM**, isolated by seL4
from the main Linux guest. This matches ADR-03's accepted design.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      seL4 MICROKERNEL                         │
│                                                               │
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────┐ │
│  │  MAIN LINUX GUEST │  │  TOR LINUX GUEST  │  │ KEY VAULT  │ │
│  │  (VM0)            │  │  (VM1 — TorPD)    │  │ PD         │ │
│  │                   │  │                   │  │            │ │
│  │  - GUI, apps      │  │  - Tor daemon     │  │ - Keys     │ │
│  │  - User workspace │  │  - Minimal Linux  │  │ - Crypto   │ │
│  │  - Network: via   │  │  - Own NIC (eth0) │  │            │ │
│  │    TorPD SOCKS    │  │  - Identity keys  │  │            │ │
│  │                   │  │    from KeyVault   │  │            │ │
│  └────────┬──────────┘  └────────┬──────────┘  └─────┬──────┘ │
│           │                        │                   │        │
│           │   IPC (shared mem)    │                   │        │
│           └────────────────────────┘                   │        │
│                                    │                   │        │
│           ┌────────────────────────┴───────────────────┘        │
│           │  (KeyVault provides Tor identity keys to TorPD)     │
│           │                                                      │
│  ┌────────▼─────────┐                                           │
│  │  FIREWALL PD     │  (future — enforces Tor-only routing)     │
│  └──────────────────┘                                           │
└─────────────────────────────────────────────────────────────┘
```

## IPC Contract

### Phase 1 (this session): Ping/Pong proof

Mirror Key Vault PD's first milestone. The TorPD component:
- Declares `consumes Ready ready` + `emits Done done` + `dataport Buf(4096)`
- On `ready_wait()`, reads shared memory
- If message is "ping", writes "pong" and emits `done`
- Prints `[tor-pd] ping received, pong sent` to serial

This proves the seL4 isolation boundary is established and IPC works.

### Phase 2 (future): SOCKS proxy relay

- Main Linux guest sends SOCKS5 connection requests via shared memory
- TorPD reads the request, makes the real TCP connection via its own NIC
- Response data flows back through shared memory
- This is the actual "Tor isolation" — the main guest never touches the network directly

### Phase 3 (future): Identity key handoff

- TorPD requests Tor identity keys from KeyVaultPD (via seL4 IPC)
- Keys never touch the main Linux guest's filesystem
- If main Linux is compromised, Tor identity is safe

## What Crosses the Isolation Boundary

| Direction | Content | Mechanism |
|-----------|---------|-----------|
| Main → TorPD | SOCKS5 request (host, port, data) | Shared memory + notification |
| TorPD → Main | SOCKS5 response (data, status) | Shared memory + notification |
| TorPD → KeyVault | Key request | seL4 RPC (like KeyVault's putchar) |
| KeyVault → TorPD | Identity key material | seL4 RPC response |
| TorPD → NIC | Raw TCP to Tor relays | Direct (TorPD has its own eth0) |

## How TorPD Differs from KeyVaultPD

| Aspect | KeyVaultPD | TorPD |
|--------|-----------|-------|
| Network I/O | None (IPC only) | Real TCP to Tor relays |
| Linux guest | No (bare seL4 component) | Yes (second Linux VM — Tor needs libc/sockets/fs) |
| Memory | ~4KB shared buffers | ~512MB (Linux + Tor) |
| IPC pattern | Request/response | Stream (SOCKS proxy relay) |
| Key storage | Stores keys | Requests keys from KeyVault |

## Build

TorPD is a CAmkES component scaffolded alongside KeyVaultPD in the same build
tree (`projects/vm-examples/apps/x86/`). The app assembly (`minimal_tor_pd.camkes`)
adds a second Linux VM component (vm1) alongside the existing vm0 + KeyVaultPD.

```
projects/vm-examples/apps/x86/tor_pd/
├── CMakeLists.txt
├── app_settings.cmake
├── minimal_tor_pd.camkes      # assembly: vm0 + vm1 + KeyVaultPD
├── src/
│   ├── tor_pd.c               # CAmkES component (ping/pong for Phase 1)
│   └── cross_vm_connections.c # wires dataports to vm1's PCI shared mem
└── include/
```

## Tests

| Test | How | Phase |
|------|-----|-------|
| TorPD alive | Serial: `[tor-pd] alive` | 1 (this session) |
| Ping/pong round-trip | Serial: `[tor-pd] ping received, pong sent` + `Result from Tor PD: pong` | 1 |
| Tor bootstraps in TorPD | Boot Tor inside vm1, verify 100% bootstrap | 2 (future) |
| SOCKS proxy works | App in vm0 connects to torproject.org via TorPD's SOCKS | 2 |
| Identity key isolation | Kill vm0, verify TorPD keeps circuits alive | 3 |

## Security

- **Isolation:** seL4 enforces memory isolation between vm0, vm1, and KeyVaultPD. Even if vm0's kernel is compromised, vm1 (Tor) and KeyVaultPD are untouched.
- **No direct network from vm0:** In Phase 2, vm0's network is removed entirely — all traffic must go through TorPD's SOCKS proxy.
- **Identity keys in KeyVault:** Tor's identity keys never touch vm1's filesystem — they're requested from KeyVaultPD via seL4 IPC at startup.
- **Kill switch:** Firewall PD (future) can drop all packets to vm1 instantly.
