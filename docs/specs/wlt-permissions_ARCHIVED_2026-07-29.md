# Spec: WLT-Permissions (Permission System)

> **Status:** Draft (ready for implementation)
> **Phase:** 3 (Month 17)
> **Dependencies:** WLT-Sandbox, WLT-Shell
> **ADR references:** None

## Purpose

WLT-Permissions manages app permissions — which apps can access network, files, camera, microphone, location, etc. It enforces the permissions declared in app manifests and provides UI for users to review and change permissions.

## Design Philosophy

**Like Android's permission system, but better:**
- Every app declares permissions in its manifest
- User sees a privacy badge per app (🟢🟡🔴)
- User can revoke permissions at any time
- Apps request permissions at runtime (not just install time)
- All permission changes logged for audit

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| App manifests | `/etc/wlt/apps/*.manifest` | INI with permissions section |
| Permission requests | Apps (via IPC) | Runtime request |
| User decisions | WLT-Shell (dialog) | Allow/deny |
| Current grants | `/var/wlt/permissions/` | Per-app permission state |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Permission grants | WLT-Sandbox | Updated sandbox config |
| Permission dialogs | WLT-Shell | "App X wants camera access" |
| Privacy badges | WLT-Shell | 🟢🟡🔴 per app |
| Audit log | `/var/log/wlt/permissions.log` | Text log |

## Dependencies

- **WLT-Sandbox** (enforces permissions at sandbox level)
- **WLT-Shell** (shows permission dialogs)
- **WLT-Settings** (permission management UI)

## What Depends on This

- All apps (all apps go through permission system)
- User (sees privacy badges, manages permissions)

## Permission Types

| Permission | Values | Description |
|-----------|--------|-------------|
| `network` | tor-only, direct, none | Network access (Tor-only recommended) |
| `filesystem` | none, home:ro, home:rw, full | File access scope |
| `camera` | allow, deny, ask | Camera access |
| `microphone` | allow, deny, ask | Microphone access |
| `location` | allow, deny, ask | GPS/location |
| `notifications` | allow, deny | Show notifications |
| `clipboard` | allow, deny, ask | Read clipboard |
| `contacts` | allow, deny, ask | Access contacts |
| `bluetooth` | allow, deny, ask | Bluetooth control |
| `usb` | allow, deny, ask | USB device access |
| `background` | allow, deny | Run in background |
| `autostart` | allow, deny | Start on boot |

## Privacy Badges

| Badge | Meaning | Criteria |
|-------|---------|----------|
| 🟢 Green | Privacy-respecting | Tor-only, sandboxed, no camera/mic/location |
| 🟡 Yellow | Review recommended | Direct network, or home:rw, or clipboard access |
| 🔴 Red | High risk | Full filesystem, or camera+mic, or background+autostart |

## Error Handling

| Error | Action |
|-------|--------|
| App requests undeclared permission | Deny, log security event |
| App manifest missing permissions section | Deny all, log warning |
| Permission database corrupt | Rebuild from manifests, log error |
| App crashes after permission revoke | Normal — app must handle denial gracefully |

## Tests

| Test | How |
|------|-----|
| App requests camera | App asks for camera, verify dialog appears |
| User grants camera | Click "Allow", verify app can access camera |
| User denies camera | Click "Deny", verify app cannot access camera |
| Revoke permission | Go to settings, revoke, verify app loses access |
| Privacy badge | Check app with tor-only shows 🟢, direct network shows 🟡 |
| Permission persistence | Grant permission, reboot, verify still granted |
| Audit log | Grant/deny permissions, verify log entries |

## Security

- **Default deny** — Apps get no permissions unless explicitly granted
- **Runtime requests** — Apps can request permissions at runtime (not just install)
- **User always in control** — Any permission can be revoked at any time
- **Audit trail** — All permission changes logged with timestamp
- **No permission inheritance** — App A's permissions don't apply to App B

## Performance

| Metric | Budget |
|--------|--------|
| Permission check | < 1ms |
| Permission dialog show | < 50ms |
| Privacy badge update | < 10ms |
| RAM | < 10MB |

## File Structure

```
src/userspace/wlt-permissions/
├── Cargo.toml
├── src/
│   ├── main.rs            # Permission manager daemon
│   ├── manifest.rs        # Read app manifests
│   ├── grants.rs          # Permission grant database
│   ├── requests.rs        # Runtime permission requests
│   ├── badges.rs          # Privacy badge calculation
│   ├── audit.rs           # Audit log
│   ├── ipc.rs             # IPC to apps + shell
│   └── error.rs
└── tests/
```

## Permission Database

```
/var/wlt/permissions/
├── firefox.json           # Firefox's granted permissions
├── terminal.json          # Terminal's permissions
├── ai-assistant.json      # AI's permissions
└── ...
```

### Example: `firefox.json`

```json
{
  "app": "firefox",
  "granted": {
    "network": "tor-only",
    "filesystem": "home:rw",
    "camera": "ask",
    "microphone": "ask",
    "location": "deny",
    "notifications": "allow",
    "clipboard": "allow",
    "background": "allow"
  },
  "history": [
    {"timestamp": "2026-06-21T10:00:00Z", "permission": "camera", "action": "granted"},
    {"timestamp": "2026-06-21T11:00:00Z", "permission": "location", "action": "denied"}
  ]
}
```

---

**Spec version:** 1.0
