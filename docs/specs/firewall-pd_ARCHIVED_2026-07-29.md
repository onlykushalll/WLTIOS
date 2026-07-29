# Spec: Firewall PD (seL4 Partition)

> **Status:** Draft (ready for implementation)
> **Phase:** 3 (Month 15)
> **Dependencies:** seL4, Microkit, sDDF (seL4 Device Driver Framework)
> **ADR references:** ADR-02 (seL4 base), ADR-03 (Tor in second VM), ADR-04 (Microkit)
> **Research:** docs/context/RESEARCH_SEL4_PARTITIONS.md

## Purpose

The Firewall PD is an seL4 Protection Domain that sits in the network data path. ALL network traffic (from both Linux guests) must pass through it. It enforces the "Tor-only" rule and implements the kill switch.

## The Critical Insight

The Firewall PD doesn't "intercept" packets — it sits **in the data path by construction**. The NIC driver PD is the only thing that talks to hardware. Packets go to the Firewall PD via sDDF queue. The Firewall forwards or drops. **Linux has no direct channel to the NIC.**

```
Main Linux Guest ──┐
                    ├──→ Firewall PD ──→ NIC Driver PD ──→ Hardware
Tor VM ────────────┘         ↑
                             │
                      Drops non-Tor traffic
                      Kill switch: drops ALL traffic
```

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Packets from Linux guests | sDDF net queue | Network packets |
| Kill switch commands | WLT-Tor-Mgr (IPC) | Block-all / allow |
| Firewall rules | Config (compiled into PD) | Allow/deny lists |
| Tor status | Tor VM (IPC) | Connected/disconnected |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Forwarded packets | NIC Driver PD (sDDF) | Network packets |
| Dropped packets | (discarded) | — |
| Status | WLT-Tor-Mgr (IPC) | Packet counts, drop counts |
| Kill switch events | WLT-Init (IPC) | Network blocked/unblocked |

## Dependencies

- **seL4** (microkernel — provides isolation)
- **Microkit** (framework for defining PDs)
- **sDDF** (seL4 Device Driver Framework — for net queues)
- **NIC Driver PD** (another seL4 partition that talks to hardware)

## What Depends on This

- All network traffic (nothing bypasses the firewall)
- WLT-Tor-Mgr (relies on firewall for kill switch)
- WLT-Net (relies on firewall for Tor-only enforcement)

## Rules (Compiled Into PD)

| Rule | Action |
|------|--------|
| Packet from Tor VM to NIC | **ALLOW** (Tor traffic exits) |
| Packet from Main Linux to NIC (direct) | **DENY** (must go through Tor VM) |
| Packet from Main Linux to Tor VM | **ALLOW** (routing through Tor) |
| Packet from Tor VM to Main Linux | **ALLOW** (Tor responses) |
| Kill switch active | **DENY ALL** (no packets pass) |
| DNS packet (not to Tor DNS port) | **DENY** (prevent DNS leak) |

## Error Handling

| Error | Action |
|-------|--------|
| Packet queue full | Drop oldest, log warning |
| NIC Driver PD unresponsive | Log critical, trigger kill switch |
| Kill switch IPC fails | Fail-closed (block all) |
| Rule parse error | Fail-closed (block all), log critical |

## Tests

| Test | How |
|------|-----|
| Tor traffic passes | Send packet from Tor VM, verify it exits NIC |
| Direct traffic blocked | Send packet from Main Linux to NIC, verify dropped |
| Kill switch | Trigger kill switch, verify ALL traffic stops |
| Kill switch recovery | Clear kill switch, verify traffic resumes |
| DNS leak prevention | Try direct DNS query, verify blocked |
| Packet counting | Send 100 packets, verify count = 100 |

## Security

- **Formally isolated** — seL4 guarantees Linux cannot bypass the firewall
- **Fail-closed** — Any error → block all traffic
- **No dynamic rules** — Rules compiled into PD at build time (cannot be modified at runtime by Linux)
- **Minimal code** — ~500 lines, easy to audit
- **No filesystem** — PD has no filesystem access (rules in memory)

## Performance

| Metric | Budget |
|--------|--------|
| Packet forwarding latency | < 10μs |
| Packet throughput | > 1 Gbps (line rate) |
| Kill switch trigger | < 100μs |
| PD RAM | < 2MB |

## Implementation Notes

- **Language:** C (bare metal — no libc in seL4 PDs)
- **Framework:** Microkit
- **Queue:** sDDF net queue (shared memory between PDs)
- **Build:** seL4 build system (cmake + ninja)

## File Structure

```
src/sel4-partitions/firewall-pd/
├── include/
│   ├── firewall.h         # Public interface
│   ├── rules.h            # Firewall rules
│   ├── queue.h            # sDDF queue interface
│   └── ipc.h              # IPC to other PDs
├── firewall.c             # Main PD logic
├── rules.c                # Rule matching
├── queue.c                # sDDF queue handling
├── ipc.c                  # IPC (kill switch, status)
├── packet.c               # Packet parsing
└── CMakeLists.txt         # Build config
```

## SDF (System Description File) Entry

```xml
<protection_domain name="firewall" id="2" priority="100">
    <program_image>firewall.elf</program_image>
    <memory_region name="net_queue" size="1MB" />
    <channel id="1" target="nic_driver" />
    <channel id="2" target="tor_vm" />
    <channel id="3" target="main_linux" />
    <channel id="4" target="wlt_tor_mgr" />
</protection_domain>
```

## Packet Flow

```
1. Main Linux guest sends a packet (wants to reach internet)
2. Packet goes to sDDF net queue (shared memory)
3. Firewall PD dequeues packet
4. Firewall checks rules:
   - Is source = Tor VM? → ALLOW (forward to NIC)
   - Is source = Main Linux + destination = Tor VM? → ALLOW
   - Is source = Main Linux + destination = NIC? → DENY (drop)
   - Kill switch active? → DENY (drop)
5. If ALLOW: enqueue to NIC Driver PD's queue
6. If DENY: drop packet, increment drop counter
7. Repeat
```

---

**Spec version:** 1.0
**Ready for implementation:** Yes (after seL4 + Linux VM boots in QEMU)
