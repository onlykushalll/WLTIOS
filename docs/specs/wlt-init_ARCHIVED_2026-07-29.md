# Spec: WLT-Init (Init System)

> **Status:** Draft (ready for implementation)
> **Phase:** 1 (Month 3)
> **Dependencies:** seL4, Linux kernel, musl libc
> **ADR references:** ADR-07 (custom init)

## Purpose

WLT-Init is PID 1 — the first userspace process in the main Linux guest. It starts all essential services in the correct order, monitors them, and handles shutdown.

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Service definitions | `/etc/wlt/services/*.service` | INI-style text |
| Boot info | seL4 / Linux kernel | boot params |
| Tor status | seL4 Firewall PD | IPC notification |
| User login | WLT-Auth | IPC message |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Started services | Linux processes | Running daemons |
| Service status | `wltctl` command | Text/JSON |
| Logs | `/var/log/wlt/` | Text files |
| Shutdown sequence | Linux kernel | `reboot()` syscall |

## Dependencies

- **seL4** (provides isolation, IPC to partitions)
- **Linux kernel** (provides processes, filesystem, signals)
- **musl libc** (C standard library)
- **Key Vault PD** (for encryption key access)
- **Firewall PD** (for Tor status)

## What Depends on This

- All userspace services (WLT-Compositor, WLT-Shell, WLT-Pkg, etc.)
- User session (login only happens after init finishes)

## Error Handling

| Error | Action |
|-------|--------|
| Service config parse error | Skip service, log error, continue |
| Service fails to start | Retry 3 times with backoff, then mark failed |
| Critical service crashes | Restart immediately (no backoff) |
| Tor not ready | Block network-dependent services until Tor is up |
| Tor dies (kill switch) | Freeze network-dependent services, wait for Tor restart |
| Filesystem mount failure | Drop to recovery shell |
| Key Vault unreachable | Refuse to start (cannot decrypt user data) |

## Tests

| Test | How |
|------|-----|
| Boot to desktop | Boot WLTIOS, verify WLT-Compositor starts |
| Service dependency order | Check logs, verify services start in correct order |
| Service crash recovery | Kill a service, verify it restarts |
| Tor dependency blocking | Stop Tor, verify network services don't start |
| Kill switch | Stop Tor, verify network services freeze |
| Config error handling | Malform a service config, verify init continues |
| Shutdown | Trigger shutdown, verify clean unmount |

## Security

- **PID 1 privileges:** WLT-Init runs as root (required for PID 1)
- **Service isolation:** Each service runs in its own cgroup + namespace
- **No network access for init:** WLT-Init doesn't make network calls (services do)
- **Config file validation:** All service configs validated before execution
- **Tor-aware:** WLT-Init will not start network services until Tor is ready

## Performance

| Metric | Budget |
|--------|--------|
| Time to start essential services | < 1 second |
| Time to reach desktop | < 2 seconds |
| WLT-Init RAM usage | < 5MB |
| WLT-Init binary size | < 100KB |

## Implementation Notes

- **Language:** C (musl libc)
- **Build:** Cross-compiled with GCC (x86_64-elf-gcc)
- **Linker:** Static linking (no shared libraries for PID 1)
- **No external dependencies:** Only musl libc + Linux syscalls

## File Structure

```
src/userspace/wlt-init/
├── include/
│   ├── init.h          # Public init interface
│   ├── service.h       # Service definition struct
│   ├── monitor.h       # Service monitoring
│   ├── dependency.h    # Dependency graph
│   ├── tor.h           # Tor status tracking
│   └── log.h           # Logging
├── init.c              # Main entry point (main → PID 1)
├── service.c           # Service loading + parsing
├── monitor.c           # Process monitoring (waitpid, signals)
├── dependency.c        # Dependency resolution
├── tor.c               # Tor status (IPC to Firewall PD)
├── mount.c             # Filesystem mounting
├── shutdown.c          # Shutdown sequence
├── log.c               # Logging to /var/log/wlt/
├── ctl.c               # wltctl command handler
└── Makefile            # Build rules
```

## Service Definition Format

```ini
# /etc/wlt/services/wlt-compositor.service
[wlt-service]
name=WLT Compositor
exec=/usr/bin/wlt-compositor
args=--config /etc/wlt/compositor.conf
depends=tor-vm,key-vault
tor_required=yes
restart_on_crash=yes
restart_backoff_ms=2000
priority=critical          # critical | normal | low
user=wlt                   # Run as this user (not root)
cgroup=memory:256MB,cpu:50%
namespace=network:tor-only,filesystem:home
```

## Boot Sequence

```
1. Linux kernel starts WLT-Init as PID 1
2. WLT-Init mounts essential filesystems:
   - /proc, /sys, /dev
   - /tmp (tmpfs, RAM-only)
   - /users (encrypted, via Key Vault PD)
3. WLT-Init loads all service definitions from /etc/wlt/services/
4. WLT-Init builds dependency graph
5. WLT-Init starts services in dependency order:
   a. key-vault (connects to seL4 Key Vault PD)
   b. tor-vm (starts Tor Linux VM via seL4)
   c. WAIT for Tor to be ready (IPC from Firewall PD)
   d. wlt-net (network manager, routes through Tor)
   e. wlt-compositor (Wayland compositor)
   f. wlt-shell (taskbar, launcher)
   g. wlt-ai (AI assistant)
   h. user-session (login screen)
6. WLT-Init enters monitoring loop:
   - waitpid() on all services
   - Restart crashed services (with backoff)
   - Monitor Tor status (freeze/thaw network services)
7. On shutdown signal:
   a. Stop user session
   b. Stop AI, shell, compositor
   c. Stop network manager
   d. Stop Tor VM
   e. Unmount filesystems
   f. sync()
   g. reboot(LINUX_REBOOT_CMD_POWER_OFF)
```

## wltctl Commands

```bash
wltctl status              # Show all services (running/stopped/failed)
wltctl start <name>        # Start a service
wltctl stop <name>         # Stop a service
wltctl restart <name>      # Restart a service
wltctl logs <name>         # Show recent logs for a service
wltctl enable <name>       # Enable service at boot
wltctl disable <name>      # Disable service at boot
wltctl tor-status          # Show Tor circuit status
wltctl kill-switch         # Manually trigger kill switch
wltctl shutdown            # Clean shutdown
wltctl reboot              # Reboot
```

---

**Spec version:** 1.0
**Ready for implementation:** Yes (after seL4 + Linux guest boots)
