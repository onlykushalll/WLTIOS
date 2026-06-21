# Spec: WLT-Shell (Desktop Shell)

> **Status:** Draft (ready for implementation)
> **Phase:** 2 (Month 9)
> **Dependencies:** WLT-Compositor, WLT-Widgets
> **ADR references:** ADR-05 (Wayland + wlroots)

## Purpose

WLT-Shell is the desktop shell — the taskbar, app launcher, notification system, and system tray. It's the primary user interface for interacting with the OS (as opposed to apps).

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Compositor events | WLT-Compositor (via IPC) | JSON messages |
| App launches | User (via launcher) | Command |
| Notifications | Apps (via D-Bus or IPC) | Notification struct |
| System status | WLT-Init (via IPC) | Service status |
| Tor status | WLT-Tor-Mgr (via IPC) | Circuit info |
| Clock | System | Time |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Taskbar UI | WLT-Compositor (layer-shell) | Rendered pixels |
| Launcher UI | WLT-Compositor (layer-shell) | Rendered pixels |
| Notifications | WLT-Compositor (layer-shell) | Rendered pixels |
| App launches | Linux processes | exec() |
| Status updates | WLT-Compositor (IPC) | JSON |

## Dependencies

- **WLT-Compositor** (renders the shell via layer-shell protocol)
- **WLT-Widgets** (button, text, icon widgets)
- **WLT-Init** (service status via IPC)
- **WLT-Tor-Mgr** (Tor circuit status)
- **musl libc**

## What Depends on This

- User interaction (taskbar, launcher are the primary interface)
- App launching (all apps launched via shell)

## Error Handling

| Error | Action |
|-------|--------|
| Compositor IPC lost | Try reconnect every 1s, show "reconnecting" |
| App launch fails | Show error notification |
| Notification queue full | Drop oldest, keep newest |
| Widget render error | Skip that widget, continue |

## Tests

| Test | How |
|------|-----|
| Taskbar visible | Boot, verify taskbar at bottom of screen |
| App launch | Click app in launcher, verify it opens |
| App switch | Click app in taskbar, verify it focuses |
| Clock updates | Wait 60s, verify clock shows correct time |
| Notification appears | Send test notification, verify it shows |
| Notification dismiss | Click notification, verify it disappears |
| Tor status indicator | Check Tor status badge updates |

## Security

- **No root:** Shell runs as `wlt` user.
- **App launch sandbox:** Apps launched via shell get sandboxed (cgroups + namespaces).
- **Notification isolation:** Notifications show app name — user can see which app sent them.
- **No clipboard access:** Shell cannot read clipboard (only focused app can).

## Performance

| Metric | Budget |
|--------|--------|
| Shell RAM | < 16MB |
| Taskbar render | < 5ms per frame |
| App launch latency | < 200ms (from click to window) |
| Notification show | < 50ms |
| Shell binary size | < 200KB |

## Implementation Notes

- **Language:** C (WLT-Widgets is C)
- **Protocol:** Wayland layer-shell (positions shell on screen edges)
- **IPC:** Unix socket to WLT-Compositor

## File Structure

```
src/userspace/wlt-shell/
├── include/
│   ├── shell.h            # Main shell struct
│   ├── taskbar.h          # Taskbar widget
│   ├── launcher.h         # App launcher
│   ├── notifications.h    # Notification system
│   ├── tray.h             # System tray
│   ├── clock.h            # Clock widget
│   └── status.h           # System status indicators
├── shell.c                # Main entry + layer-shell setup
├── taskbar.c              # Taskbar (running apps, app switcher)
├── launcher.c             # App launcher (search + launch)
├── notifications.c        # Notification queue + display
├── tray.c                 # System tray (Tor status, network, battery)
├── clock.c                # Clock widget
├── status.c               # CPU, RAM, network indicators
├── ipc.c                  # IPC to compositor + init
├── app-launch.c           # App launching + sandboxing
└── Makefile
```

## Taskbar Layout

```
┌─────────────────────────────────────────────────────────────┐
│ [Start] [App1] [App2] [App3]     ...     [Tor] [Net] [Bat] [Clock] │
└─────────────────────────────────────────────────────────────┘
```

| Element | Purpose |
|---------|---------|
| Start button | Opens app launcher |
| App buttons | Running apps (click to focus, right-click for menu) |
| Tor indicator | 🟢 Tor active / 🔴 Tor down (kill switch) |
| Network indicator | WiFi strength / Ethernet / Disconnected |
| Battery indicator | Percentage + charging status |
| Clock | Current time + date |

## App Launcher

```
┌─────────────────────────────┐
│  🔍 Search apps...          │
├─────────────────────────────┤
│  📁 File Manager            │
│  🌐 Browser                 │
│  💻 Terminal                │
│  📝 Text Editor             │
│  ⚙️ Settings                │
│  🎬 Media Player            │
│  🖼️ Image Viewer            │
│  🧮 Calculator              │
└─────────────────────────────┘
```

- Type to search
- Arrow keys to navigate
- Enter to launch
- Esc to close

## Notification System

```
┌─────────────────────────────┐
│  📧 New Message             │
│  From: alice@onion.address  │
│  "Hey, are you there?"      │
│                    [Dismiss] │
└─────────────────────────────┘
```

- Appears top-right
- Auto-dismiss after 5s (configurable)
- Click to open app
- Queue (max 5 visible)
- Privacy mode: don't show message content in notification

---

**Spec version:** 1.0
**Ready for implementation:** Yes (after WLT-Compositor is working)
