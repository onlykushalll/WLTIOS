# Spec: WLT-Tor-Mgr (Tor Control Manager)

> **Status:** Draft (ready for implementation)
> **Phase:** 3 (Month 16)
> **Dependencies:** Tor VM (second Linux guest), Firewall PD
> **ADR references:** ADR-03 (Tor in second VM)

## Purpose

WLT-Tor-Mgr is the control panel for Tor. It communicates with the Tor daemon running in the second Linux VM, manages circuits, handles identity switching, and provides status to the rest of the OS.

## Architecture Context

Per ADR-03, Tor runs in a second Linux VM (not natively in seL4). WLT-Tor-Mgr runs in the main Linux guest and communicates with the Tor VM via seL4 IPC (through the Firewall PD).

```
Main Linux Guest
  └── WLT-Tor-Mgr (this module)
        ↓ IPC (via seL4)
Tor VM (second Linux)
  └── Tor daemon
        ↓ Network (via seL4)
Firewall PD
  └── NIC
```

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Tor daemon status | Tor VM (via IPC) | Connected, circuit count, bandwidth |
| Circuit info | Tor VM (via IPC) | Node list, country, latency |
| User commands | WLT-Shell, CLI | New identity, circuit switch |
| Config | `/etc/wlt/tor.conf` | INI-style text |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Tor commands | Tor VM (via IPC) | New circuit, new identity, config change |
| Status updates | WLT-Shell (tray) | 🟢/🔴 Tor status |
| Circuit viewer | WLT-Settings UI | Circuit visualization |
| Bandwidth stats | WLT-Settings UI | Up/down bandwidth |

## Dependencies

- **Tor VM** (second Linux guest running Tor daemon)
- **Firewall PD** (routes traffic, provides IPC path)
- **WLT-Net** (uses Tor SOCKS proxy for all network)

## What Depends on This

- WLT-Shell (shows Tor status in tray)
- WLT-Net (uses Tor SOCKS proxy)
- WLT-Settings (Tor configuration UI)
- User (can view circuits, switch identity)

## Features

| Feature | Description |
|---------|-------------|
| **Status indicator** | 🟢 Tor active / 🔴 Tor down / 🟡 Connecting |
| **Circuit viewer** | See current Tor circuit (entry, middle, exit nodes) |
| **New identity** | Get new Tor circuits (clears cookies, new IP) |
| **Bandwidth monitor** | Real-time up/down bandwidth through Tor |
| **Exit node selection** | Choose exit country (or auto) |
| **Bridge configuration** | Configure Tor bridges (for censorship circumvention) |
| **Kill switch integration** | If Tor dies, trigger kill switch via Firewall PD |

## Error Handling

| Error | Action |
|-------|--------|
| Tor VM unreachable | Show 🔴, trigger kill switch (block all network) |
| Tor daemon crashed | Restart Tor VM (via seL4), show 🟡 "reconnecting" |
| All circuits failed | Show warning, try to rebuild circuits |
| Bandwidth limit hit | Throttle, show warning |
| Bridge connection failed | Try fallback bridges, then fail |

## Tests

| Test | How |
|------|-----|
| Tor status | Check status indicator shows 🟢 when Tor running |
| Circuit viewer | Open viewer, verify 3 nodes shown |
| New identity | Click "New Identity", verify IP changes |
| Kill switch | Stop Tor VM, verify all network blocked |
| Tor recovery | Restart Tor VM, verify network resumes |
| Exit node selection | Select country, verify exit node changes |
| Bridge config | Add bridge, verify Tor uses it |

## Security

- **No direct Tor daemon access** — WLT-Tor-Mgr talks to Tor VM via seL4 IPC, not direct socket
- **Identity keys in Key Vault PD** — Tor's identity keys stored in seL4 partition, not in Tor VM
- **Kill switch enforced** — If Tor dies, Firewall PD blocks ALL traffic instantly
- **No logs** — WLT-Tor-Mgr does NOT log Tor circuits, browsing, or any identifying info

## Performance

| Metric | Budget |
|--------|--------|
| Status update interval | 1 second |
| New identity time | < 5 seconds |
| Circuit viewer refresh | < 500ms |
| Kill switch trigger | < 100ms (delegated to Firewall PD) |
| RAM | < 15MB |

## File Structure

```
src/userspace/wlt-tor-mgr/
├── Cargo.toml
├── src/
│   ├── main.rs            # Daemon entry
│   ├── status.rs          # Tor status monitoring
│   ├── circuits.rs        # Circuit viewer
│   ├── identity.rs        # New identity logic
│   ├── bridges.rs         # Bridge configuration
│   ├── bandwidth.rs       # Bandwidth monitoring
│   ├── kill_switch.rs     # Kill switch integration
│   ├── ipc.rs             # IPC to Tor VM (via seL4)
│   ├── config.rs          # Config parsing
│   └── error.rs
└── tests/
```

## CLI Commands

```bash
wlttor status              # Show Tor status
wlttor circuits            # Show current circuits
wlttor new-identity        # Get new Tor identity
wlttor set-exit <country>  # Set exit country (e.g., "DE" for Germany)
wlttor set-exit auto       # Auto-select exit
wlttor add-bridge <addr>   # Add Tor bridge
wlttor kill-switch         # Manually trigger kill switch
wlttor bandwidth           # Show bandwidth stats
```

## Config Format

```ini
[wlt-tor-mgr]
socks_port = 9050          # Tor SOCKS port (in Tor VM)
dns_port = 5353            # Tor DNS port
control_port = 9051        # Tor control port
status_interval = 1        # Seconds between status checks

[bridges]
enabled = no               # Use bridges (for censored networks)
# Bridge addresses (if enabled)
bridges =                  # One per line

[exit]
mode = auto                # auto | country
country =                  # ISO country code (if mode = country)
```

---

**Spec version:** 1.0
