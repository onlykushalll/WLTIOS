# Spec: WLT-Net (Network Manager)

> **Status:** Draft (ready for implementation)
> **Phase:** 2 (Month 8)
> **Dependencies:** WLT-Init, Tor VM, Firewall PD
> **ADR references:** ADR-03 (Tor in second VM), ADR-07 (custom init)

## Purpose

WLT-Net manages all network connections (WiFi, Ethernet, VPN) and ensures ALL traffic routes through Tor. It is the only component (besides the Firewall PD) that can configure network interfaces.

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| WiFi scan results | Linux nl80211 | SSID list with signal strength |
| Ethernet status | Linux netlink | Link up/down events |
| Tor status | WLT-Tor-Mgr (IPC) | Connected/disconnected |
| User config | `/etc/wlt/network.conf` | INI-style text |
| Firewall rules | seL4 Firewall PD (IPC) | Allow/deny lists |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| WiFi connection | Linux wpa_supplicant | wpa_supplicant config |
| Ethernet config | Linux iproute2 | IP address, routes |
| Tor SOCKS proxy | WLT-Shell, apps | `socks5://127.0.0.1:9050` |
| Network status | WLT-Shell (tray) | Connected/disconnected, SSID |
| Kill switch trigger | seL4 Firewall PD (IPC) | Block-all command |

## Dependencies

- **WLT-Init** (started by init, runs as `wlt-net` user)
- **Tor VM** (all traffic routes through this)
- **Firewall PD** (enforces Tor-only rule at seL4 level)
- **wpa_supplicant** (WiFi authentication)
- **Rust crates:** neli (netlink), tokio (async)

## What Depends on This

- WLT-Shell (shows network status in tray)
- All apps (use Tor SOCKS proxy for network)
- WLT-Update (downloads updates via Tor)

## Error Handling

| Error | Action |
|-------|--------|
| WiFi hardware missing | Log warning, WiFi disabled |
| WiFi scan fails | Retry 3 times, then show "no networks" |
| WiFi connection fails | Retry with backoff, show error notification |
| Tor disconnects | **Trigger kill switch immediately** — block all network |
| Tor reconnects | Re-allow traffic, notify user |
| Ethernet cable unplugged | Switch to WiFi (if available), log event |
| No network available | Show "offline" status, queue pending requests |

## Tests

| Test | How |
|------|-----|
| WiFi scan | Trigger scan, verify SSID list returned |
| WiFi connect | Connect to test AP, verify connection |
| WiFi disconnect | Disconnect, verify status updates |
| Ethernet connect | Plug in cable, verify auto-config |
| Tor routing | Verify all traffic goes through Tor (no leaks) |
| Kill switch | Stop Tor, verify ALL traffic stops |
| Kill switch recovery | Restart Tor, verify traffic resumes |
| MAC randomization | Reboot, verify MAC changed |

## Security

- **Tor-only enforcement:** WLT-Net cannot route traffic directly to NIC. Firewall PD blocks everything except Tor VM traffic.
- **MAC randomization:** MAC address randomized on every boot + every WiFi connect
- **No direct DNS:** DNS goes through Tor (no system DNS resolver)
- **No telemetry:** WLT-Net never sends data anywhere (it only configures interfaces)
- **Runs as `wlt-net` user** (not root, but with capabilities for network config)

## Performance

| Metric | Budget |
|--------|--------|
| WiFi scan | < 3 seconds |
| WiFi connect | < 5 seconds |
| Kill switch trigger | < 100ms |
| MAC randomization | < 50ms |
| WLT-Net RAM | < 20MB |

## Implementation Notes

- **Language:** Rust
- **Async runtime:** tokio
- **WiFi:** wpa_supplicant via D-Bus or control socket
- **Ethernet:** iproute2 commands (ip, ethtool)
- **Tor proxy:** Forwards to Tor VM's SOCKS port (9050)

## File Structure

```
src/userspace/wlt-net/
├── Cargo.toml
├── src/
│   ├── main.rs            # Entry point
│   ├── wifi.rs            # WiFi scanning + connection
│   ├── ethernet.rs        # Ethernet management
│   ├── tor.rs             # Tor SOCKS proxy management
│   ├── mac.rs             # MAC address randomization
│   ├── firewall_ipc.rs    # IPC to seL4 Firewall PD
│   ├── kill_switch.rs     # Kill switch logic
│   ├── config.rs          # Config parsing
│   ├── status.rs          # Status reporting
│   └── error.rs
└── tests/
```

## CLI Commands

```bash
wltnet status              # Show network status
wltnet wifi scan           # Scan for WiFi networks
wltnet wifi connect <ssid> # Connect to WiFi
wltnet wifi disconnect     # Disconnect WiFi
wltnet wifi forget <ssid>  # Forget saved WiFi
wltnet tor status          # Show Tor status
wltnet tor new-identity    # Get new Tor identity (new circuits)
wltnet kill-switch         # Manually trigger kill switch
wltnet mac randomize       # Randomize MAC address now
```

## Config Format

```ini
[wlt-net]
tor_socks_port = 9050
tor_dns_port = 5353
kill_switch = enabled      # enabled | disabled
mac_randomize = on_boot    # on_boot | always | never

[wifi]
auto_connect = yes         # Auto-connect to saved networks
scan_interval = 30         # Seconds between background scans

[ethernet]
auto_config = yes          # Use DHCP
```

---

**Spec version:** 1.0
