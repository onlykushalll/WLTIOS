# Spec: WLT-Settings (System Settings)

> **Status:** Draft (ready for implementation)
> **Phase:** 2 (Month 10)
> **Dependencies:** WLT-Compositor, WLT-Widgets, all WLT-* modules
> **ADR references:** ADR-05 (Wayland + wlroots)

## Purpose

WLT-Settings is the central settings application. Users configure everything from here: network, display, security, privacy, accounts, updates, and more. It's the "Control Panel" of WLTIOS.

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| User input | WLT-Compositor | Mouse + keyboard |
| Current settings | Various modules | Current config values |
| System status | WLT-Init (IPC) | Service status |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Setting changes | Various modules (IPC) | New config values |
| Config files | `/etc/wlt/*.conf` | Updated INI files |
| UI | WLT-Compositor (surface) | Rendered pixels |

## Dependencies

- **WLT-Compositor** (Wayland surface)
- **WLT-Widgets** (tabs, sliders, toggles, dropdowns)
- **All WLT-* modules** (settings reads/writes their configs)

## What Depends on This

- User (primary configuration interface)

## Settings Categories

| Category | What It Controls |
|----------|-----------------|
| **Network** | WiFi, Ethernet, Tor config, kill switch, MAC randomization |
| **Display** | Resolution, scaling, brightness, multi-monitor, night light |
| **Sound** | Volume, output device, input device, notifications |
| **Security** | Disk encryption, firewall, sandboxing, verified boot |
| **Privacy** | Tor circuits, identity, telemetry (always off), data retention |
| **Accounts** | User accounts, login options, biometrics |
| **Updates** | Update schedule, rollback, package sources |
| **Apps** | Installed apps, permissions, defaults |
| **System** | Date/time, language, keyboard layout, power |
| **About** | OS version, hardware info, license |

## Error Handling

| Error | Action |
|-------|--------|
| Config write fails | Show error, revert to previous value |
| Invalid value | Show validation error, don't save |
| Module unreachable | Show "service unavailable", disable that section |
| Permission denied | Show "requires admin" prompt |

## Tests

| Test | How |
|------|-----|
| Change wallpaper | Select wallpaper, verify desktop updates |
| Change resolution | Select resolution, verify display changes |
| Toggle kill switch | Enable kill switch, verify network blocks |
| Change Tor identity | Click "New Identity", verify circuits change |
| Add WiFi network | Enter SSID + password, verify connects |
| Update schedule | Change update time, verify schedule updates |

## Security

- **Runs as user** for most settings
- **Requires auth** for security-sensitive changes (disk encryption, firewall)
- **No network access** — Settings app doesn't make network calls (modules do)
- **Config validation** — All values validated before saving

## Performance

| Metric | Budget |
|--------|--------|
| Open settings | < 200ms |
| Switch category | < 50ms |
| Save setting | < 100ms |
| RAM | < 20MB |

## File Structure

```
src/userspace/wlt-settings/
├── include/
│   ├── settings.h        # Main settings app
│   ├── categories.h      # Category definitions
│   ├── widgets.h         # Custom setting widgets
│   └── config_io.h       # Config read/write
├── settings.c            # Main entry
├── network_panel.c       # Network settings UI
├── display_panel.c       # Display settings UI
├── sound_panel.c         # Sound settings UI
├── security_panel.c      # Security settings UI
├── privacy_panel.c       # Privacy settings UI
├── accounts_panel.c      # User accounts UI
├── updates_panel.c       # Update settings UI
├── apps_panel.c          # App management UI
├── system_panel.c        # System settings UI
├── about_panel.c         # About / version info
├── config_io.c           # Config file read/write
└── Makefile
```

---

**Spec version:** 1.0
