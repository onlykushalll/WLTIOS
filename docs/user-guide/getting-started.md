# WLTIOS User Guide: Getting Started

## What is WLTIOS?

WLTIOS (Windows + Linux + Tails + Inspired OS) is a privacy-first desktop operating system built on seL4 microkernel isolation. It combines:
- **Windows-like** usability (familiar GUI, taskbar, file manager)
- **Linux-like** efficiency (low RAM, modular services, fast boot)
- **Tails-like** anonymity (Tor everywhere, amnesic mode, MAC randomization)
- **Kairos AI** — a built-in JARVIS-style guardian assistant

## System Requirements

- x86-64 processor (Intel VT-x or AMD-V required for seL4 VM)
- 2GB RAM minimum (4GB recommended)
- 4GB disk space (for install) or 8GB USB drive (for Live USB)
- USB boot support in BIOS/UEFI

## Installation

### Option 1: Live USB (Recommended — No installation needed)

1. Download `wltios-latest.iso` from the releases page
2. Flash to a USB drive using `dd` or Rufus:
   ```bash
   dd if=wltios-latest.iso of=/dev/sdX bs=4M status=progress
   ```
3. Boot from the USB drive (press F12/F2/Del at startup for boot menu)
4. WLTIOS boots directly into the desktop — nothing is written to your disk

### Option 2: Hard Drive Installation

1. Boot from Live USB
2. Open the installer from the desktop
3. Follow the guided setup:
   - Choose disk
   - Set encryption passphrase (LUKS)
   - Create user account
4. Reboot — WLTIOS is installed

## First Boot

When WLTIOS boots, you'll see:
- Boot splash with progress bar
- Login screen (if persistence enabled) or direct desktop (Live USB)
- Desktop with taskbar at bottom

### The Desktop

- **Taskbar** (bottom): Shows running apps, system tray (Tor status, network, clock)
- **App Launcher** (bottom-left or Super key): Search and launch applications
- **System Tray**: Click Tor icon to view circuit, click network for WiFi/Tor settings

### Key Shortcuts

| Shortcut | Action |
|---|---|
| Super (Windows key) | Open app launcher |
| Alt+Tab | Switch between windows |
| Super+D | Show desktop |
| Ctrl+Alt+T | Open terminal |
| Super+E | Open file manager |
| Super+S | Open settings |
| "Wake up, Buddy!" | Activate Kairos AI |

## Using Tor

All network traffic routes through Tor by default. The system tray shows:
- 🟢 Green: Tor connected, all traffic anonymized
- 🟡 Yellow: Tor connecting, some traffic may leak
- 🔴 Red: Tor disconnected, network kill switch active (no traffic flows)

### Getting a New Identity

Click the Tor icon in the system tray → "New Identity". This changes your Tor circuit, giving you a new exit node and IP address.

### Browsing .onion Sites

Open the Tor Browser (pre-installed). It's configured for maximum privacy:
- No JavaScript by default
- No browser fingerprinting
- All traffic through Tor

## Privacy Features

### Amnesic Mode (Live USB)

In Live USB mode, nothing is written to disk:
- All files are stored in RAM (tmpfs)
- On reboot, everything is gone — no trace
- To save files, use the encrypted persistence partition

### MAC Randomization

Your network card's MAC address is randomized on every boot. This prevents tracking across sessions.

### Kill Switch

If Tor disconnects (network failure, evil maid attack), ALL network traffic stops instantly. No data leaks outside Tor.

### Privacy Badges

Every app shows a privacy badge:
- 🟢 Green: App respects privacy (no network, no file access beyond sandbox)
- 🟡 Yellow: App has some access (network via Tor, limited file access)
- 🔴 Red: App has broad access (direct network, full file access) — use with caution

## Kairos AI

Kairos is your built-in AI guardian. Say **"Wake up, Buddy!"** to activate it.

### What Kairos Can Do

- **File search**: "Find my PDF from Monday"
- **Privacy analysis**: "Which apps are spying on me?"
- **Anonymity coach**: "Am I leaking DNS?"
- **System control**: "Connect to WiFi" or "Start Tor Browser"
- **Active defense**: Kairos monitors for attacks and warns you proactively

### Privacy

- Kairos runs a LOCAL model by default (VibeThinker-3B) — no data leaves your machine
- You can add a cloud API key (Gemini, Claude, etc.) for more complex tasks
- Even with cloud APIs, Kairos filters all requests through a privacy pipeline (strips PII, routes through Tor)

## Settings

Open Settings (Super+S) to configure:
- **Network**: WiFi, Tor config, kill switch
- **Display**: Resolution, brightness, multi-monitor
- **Sound**: Volume, output device
- **Privacy**: Amnesic mode, MAC randomization, app permissions
- **Security**: Disk encryption, boot verification, Kairos guardian
- **Account**: User settings, password

## Troubleshooting

### Won't boot from USB
- Verify USB was flashed correctly (`dd` or Rufus in DD mode)
- Check BIOS/UEFI boot order
- Disable Secure Boot (WLTIOS uses its own signed bootloader)

### Tor won't connect
- Check if your ISP blocks Tor (use a bridge: Settings → Network → Tor → Bridges)
- Try "New Identity" from the Tor tray icon
- Check firewall isn't blocking Tor ports

### Kill switch blocks everything
- This means Tor is disconnected. Click the red Tor icon to see why
- If using a bridge, verify the bridge is still working
- Restart Tor: `sudo wltctl restart tor`

### Kairos doesn't respond to hotword
- Check microphone is connected and not muted
- Verify Kairos is enabled: Settings → Security → Kairos
- Check Kairos logs: `wltctl status kairos`

## Getting Help

- GitHub: https://github.com/onlykushalll/WLTIOS/issues
- Documentation: https://github.com/onlykushalll/WLTIOS/tree/main/docs
- Community forum: (coming soon)
