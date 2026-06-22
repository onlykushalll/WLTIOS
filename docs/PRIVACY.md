# WLTIOS Privacy Policy

**Last updated:** 2026-06-22

## TL;DR

WLTIOS collects **zero data**. No telemetry, no analytics, no crash reports, no usage statistics. Your data never leaves your machine unless you explicitly send it.

## Data Collection

### What WLTIOS Collects

**Nothing.**

WLTIOS does not:
- Phone home
- Send telemetry
- Send crash reports
- Send usage statistics
- Send hardware information
- Send IP addresses
- Send MAC addresses
- Send anything to any server

### What WLTIOS Stores Locally

| Data | Where | Encrypted? | Survives Reboot? |
|------|-------|-----------|-----------------|
| User files | /home (LUS encrypted) | Yes | Yes (if persistence enabled) |
| App data | /var/lib (LUKS encrypted) | Yes | Yes (if persistence enabled) |
| System logs | /var/log (tmpfs) | No | **No** (amnesic — gone on reboot) |
| AI conversation history | /var/lib/wlt-ai/memory.enc | Yes (Key Vault PD) | Optional (amnesic by default) |
| Network config | /etc/wltios/ (LUKS encrypted) | Yes | Yes (if persistence enabled) |

In Live USB (amnesic) mode, **nothing survives a reboot**. All writable paths are tmpfs (RAM).

## Network Activity

### Outbound Connections

All outbound traffic routes through **Tor** (3-hop onion). No direct connections are made except:

1. **Tor directory authorities** — to bootstrap the Tor network (through Tor itself after bootstrap)
2. **Tor bridges** — if configured (obfuscated connections to bridge relays)
3. **Package repository** — when you explicitly run `wlt-pkg update` (through Tor)
4. **Cloud AI API** — only if you explicitly configure a cloud API key in Kairos settings

### Inbound Connections

WLTIOS accepts **zero inbound connections** by default. No services listen on external interfaces.

### Kill Switch

If Tor disconnects for any reason, **ALL network traffic stops instantly**. The kill switch is enforced by the seL4 Firewall PD — Linux cannot disable it.

## Kairos AI Privacy

### Local Mode (Default)

Kairos uses a **local LLM** (VibeThinker-3B, 3B parameters, Q4 GGUF ~2GB). All inference happens on your machine. No data leaves.

### Cloud Mode (Optional, User-Enabled)

If you add a cloud API key (Gemini, Claude, GLM):

1. **Privacy filter** strips personally identifiable information before sending:
   - File paths → replaced with `[PATH]`
   - IP addresses → replaced with `[IP]`
   - Email addresses → replaced with `[EMAIL]`
   - Credit card numbers → replaced with `[REDACTED]`

2. **Tor routing** — all cloud API requests route through Tor (your real IP is hidden)

3. **No persistence** — cloud API keys are stored in the Key Vault PD (encrypted, volatile)

4. **Amnesic** — in amnesic mode, the API key is gone on reboot

Even with cloud mode enabled, the cloud provider cannot:
- See your real IP (Tor)
- Receive file paths or system information (privacy filter)
- Correlate requests across sessions (Tor + no persistent identifiers)

## MAC Address

Your network card's MAC address is **randomized on every boot**. This prevents:
- Tracking across sessions
- Correlation between user sessions
- Hardware fingerprinting

## DNS

All DNS queries route through **Tor**. No DNS leaks are possible:
- The Firewall PD enforces DNS-over-Tor at the packet level
- Linux cannot bypass this (seL4 isolation)

## Disk Encryption

All disk partitions (root, home, swap) are encrypted with **LUKS2** (AES-256-XTS). The LUKS master key:
- Is generated at install time
- Is sealed to TPM 2.0 (or encrypted with user passphrase)
- Is stored on the EFI System Partition (outside LUKS, but encrypted)
- Is unsealed at boot by WLT-Boot (before Linux starts)
- Is mapped into the Key Vault PD by seL4 (never in Linux kernel memory)

See [Key Vault PD spec](docs/specs/key-vault-pd.md) section "Key Persistence Architecture" for details.

## WebRTC

WebRTC is **disabled** in the Tor Browser to prevent IP leaks. The browser config (`prefs.js`) sets:
```
media.peerconnection.enabled = false
```

## What You Can Do

1. **Use Live USB mode** — nothing touches disk
2. **Verify the kill switch** — check the Tor tray icon is green before browsing
3. **Use New Identity** regularly — click Tor tray icon → "New Identity"
4. **Check privacy badges** — 🟢 safe, 🟡 caution, 🔴 risk
5. **Ask Kairos** — "Am I leaking DNS?" or "Which apps are spying?"

## Contact

Privacy questions: Open an issue at https://github.com/onlykushalll/WLTIOS/issues

## License

This privacy policy is part of the WLTIOS project, licensed under GPL-3.0.
