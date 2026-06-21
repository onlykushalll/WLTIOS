# Spec: WLT-Sandbox (App Sandboxing)

> **Status:** Draft (ready for implementation)
> **Phase:** 2 (Month 11-12)
> **Dependencies:** WLT-Init, Linux cgroups/namespaces
> **ADR references:** ADR-07 (custom init)

## Purpose

WLT-Sandbox isolates applications from each other and from the system. Every app runs in its own sandbox with restricted permissions. If an app is compromised, it cannot access other apps or system resources.

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| App manifest | `/etc/wlt/apps/<app>.manifest` | INI-style permissions |
| Launch request | WLT-Shell, WLT-Init | App name + args |
| Resource limits | App manifest | CPU, RAM, disk quotas |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Sandbox setup | Linux kernel | cgroup + namespace config |
| Sandboxed process | Linux | Running app in isolated environment |
| Permission grants | App (via IPC) | Granted/denied responses |
| Audit log | `/var/log/wlt/sandbox/` | Text logs |

## Dependencies

- **Linux cgroups v2** (resource limits)
- **Linux namespaces** (pid, net, mnt, ipc, uts, user)
- **Linux seccomp** (syscall filtering)
- **WLT-Init** (starts sandbox manager)

## What Depends on This

- All apps (every app is sandboxed)
- WLT-Shell (launches apps with sandbox)
- WLT-Permissions (permission system builds on sandbox)

## Error Handling

| Error | Action |
|-------|--------|
| App manifest missing | Refuse to launch, show error |
| App manifest invalid | Refuse to launch, show error |
| Sandbox setup fails | Refuse to launch, show error |
| App exceeds RAM limit | Kill app (OOM killer), notify user |
| App tries forbidden syscall | Kill app (seccomp), log security event |
| App tries forbidden file access | Deny, log security event |

## Tests

| Test | How |
|------|-----|
| App isolation | App A writes file, App B tries to read — verify denied |
| RAM limit | App tries to allocate > limit, verify killed |
| CPU limit | App tries to use > limit, verify throttled |
| File access | App without file permission tries to read /users, verify denied |
| Network access | App without network permission tries to connect, verify denied |
| Camera access | App without camera permission tries to open /dev/video, verify denied |
| Seccomp | App tries forbidden syscall, verify killed |

## Security

- **Every app sandboxed** — No exceptions. Even system apps run sandboxed.
- **Default deny** — Apps get no permissions unless explicitly granted in manifest.
- **seccomp filter** — Apps can only call whitelisted syscalls.
- **Namespace isolation** — Apps cannot see other processes, network, or filesystem (except granted paths).
- **Resource limits** — Apps cannot exhaust system resources.

## Performance

| Metric | Budget |
|--------|--------|
| Sandbox setup time | < 50ms |
| Sandbox overhead | < 2% CPU, < 5MB RAM |
| Permission check | < 1ms |

## File Structure

```
src/userspace/wlt-sandbox/
├── Cargo.toml
├── src/
│   ├── main.rs            # Sandbox manager
│   ├── cgroup.rs          # cgroup v2 setup
│   ├── namespace.rs       # Namespace setup
│   ├── seccomp.rs         # Syscall filtering
│   ├── manifest.rs        # App manifest parsing
│   ├── launch.rs          # Launch app in sandbox
│   ├── monitor.rs         # Monitor sandboxed apps
│   ├── audit.rs           # Security audit log
│   └── error.rs
└── tests/
```

## App Manifest Format

```ini
# /etc/wlt/apps/firefox.manifest
[app]
name = Firefox
exec = /usr/bin/firefox
icon = /usr/share/icons/firefox.png

[permissions]
network = tor-only          # tor-only | direct | none
filesystem = home:rw        # none | home:ro | home:rw | full
camera = ask                # allow | deny | ask
microphone = deny           # allow | deny | ask
location = deny             # allow | deny | ask
notifications = allow       # allow | deny
clipboard = ask             # allow | deny | ask

[resources]
memory_mb = 512             # Max RAM
cpu_percent = 50            # Max CPU
disk_mb = 1024              # Max disk write
network_kbps = 0            # 0 = unlimited

[seccomp]
# Allowed syscall groups
allowed = basic, network, filesystem
# basic: read, write, open, close, mmap, munmap, brk, exit, etc.
# network: socket, connect, bind, listen, accept, send, recv (only via Tor SOCKS)
# filesystem: stat, fstat, readdir (only on granted paths)
```

## Sandbox Setup Flow

```
1. App launch requested (e.g., user clicks Firefox in launcher)
2. WLT-Sandbox reads /etc/wlt/apps/firefox.manifest
3. Validate manifest (format, permissions)
4. Create cgroup for app:
   - memory.max = 512MB
   - cpu.max = 50%
5. Create namespaces:
   - pid namespace (app sees only itself)
   - net namespace (routes through Tor SOCKS only)
   - mnt namespace (can only see granted paths)
   - ipc namespace (isolated IPC)
6. Apply seccomp filter (whitelist allowed syscalls)
7. Set up filesystem:
   - Mount /usr (read-only)
   - Mount /users/<user> (read-write, if granted)
   - Mount /tmp (private tmpfs)
   - Mount /dev (limited devices)
8. Exec the app binary in the sandbox
9. Monitor the app:
   - Watch for OOM (kill if exceeds RAM)
   - Watch for seccomp violations (kill + log)
   - Watch for exit (clean up sandbox)
```

---

**Spec version:** 1.0
