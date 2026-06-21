# Spec: WLT-FileManager (File Manager)

> **Status:** Draft (ready for implementation)
> **Phase:** 1 (Month 4)
> **Dependencies:** WLT-Compositor, WLT-Widgets, WLT-Crypto
> **ADR references:** ADR-05 (Wayland + wlroots)

## Purpose

WLT-FileManager is the graphical file browser. It lets users navigate, open, copy, move, delete, and encrypt files. It includes unique features: Tor downloads, per-file encryption, and privacy badges.

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Filesystem | Linux VFS | Files + directories |
| User input | WLT-Compositor | Mouse + keyboard events |
| Tor downloads | WLT-Net (Tor SOCKS) | Downloaded files |
| Encryption status | WLT-Crypto | Per-file encrypted/plain |
| Config | `/etc/wlt/filemanager.conf` | INI-style text |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| File browser UI | WLT-Compositor (surface) | Rendered pixels |
| File operations | Linux VFS | create, copy, move, delete |
| Tor downloads | WLT-Net (Tor SOCKS) | HTTP GET via Tor |
| Encrypt requests | WLT-Crypto (IPC) | File path + key ID |

## Dependencies

- **WLT-Compositor** (Wayland surface)
- **WLT-Widgets** (buttons, list, icons)
- **WLT-Crypto** (per-file encryption, secure delete)
- **WLT-Net** (Tor downloads)
- **musl libc**

## What Depends on This

- User (primary file management interface)

## Unique Features (Not in Other File Managers)

| Feature | Description |
|---------|-------------|
| **Tor Download** | Paste a URL → file downloads via Tor (no IP leak) |
| **Per-file Encryption** | Right-click → "Encrypt" → file encrypted with Key Vault key |
| **Privacy Badges** | Files show badge: 🔒 encrypted, 🌐 downloaded via Tor, ⚠️ sensitive |
| **Secure Delete** | Shift+Delete → shred (overwrite 3x + unlink) |
| **Version History** | Every save creates a snapshot (if persistence enabled) |
| **AI Search** | "Find the PDF I downloaded last week" → AI searches by description |
| **Git Integration** | Files in git repos show status (modified, staged, etc.) |

## Error Handling

| Error | Action |
|-------|--------|
| Permission denied | Show error toast, don't crash |
| Disk full | Show error, cancel operation |
| File not found | Refresh view, show toast |
| Tor download fails | Retry 3 times, show error |
| Encryption fails | Show error, leave file unencrypted |
| Decrypt fails (wrong key) | Show error, don't corrupt file |

## Tests

| Test | How |
|------|-----|
| Navigate | Click folders, verify contents update |
| Copy file | Copy file, verify duplicate exists |
| Move file | Move file, verify original gone, new exists |
| Delete file | Delete file, verify gone |
| Secure delete | Shift+Delete, verify file shredded (no recover) |
| Encrypt file | Right-click → Encrypt, verify badge appears |
| Decrypt file | Double-click encrypted file, verify decrypted |
| Tor download | Paste URL, verify file downloads via Tor |
| AI search | Search "PDF from last week", verify results |

## Security

- **Runs as user** (not root) — Can only access files user owns
- **Sandboxed** — Like all apps, runs in cgroup + namespace
- **Secure delete default** — Shift+Delete shreds. Regular Delete moves to trash.
- **Encryption at rest** — Encrypted files stay encrypted until explicitly opened
- **No telemetry** — File manager never reports what files exist

## Performance

| Metric | Budget |
|--------|--------|
| Directory listing (1000 files) | < 200ms |
| File copy (100MB) | < 1 second (disk-bound) |
| Thumbnail generation | < 50ms per image |
| Tor download start | < 2 seconds |
| RAM | < 30MB |

## File Structure

```
src/userspace/wlt-filemanager/
├── include/
│   ├── filemanager.h     # Main struct
│   ├── browser.h         # Directory browsing
│   ├── operations.h      # Copy, move, delete
│   ├── thumbnails.h      # Image thumbnails
│   ├── tor_download.h    # Tor download manager
│   ├── encryption.h      # Encryption UI
│   ├── search.h          # File search + AI search
│   └── config.h
├── filemanager.c         # Main entry
├── browser.c             # Directory navigation
├── operations.c          # File operations
├── thumbnails.c          # Thumbnail generation
├── tor_download.c        # Download via Tor
├── encryption.c          # Encrypt/decrypt UI
├── search.c              # File search
├── secure_delete.c       # shred wrapper
├── config.c
└── Makefile
```

## UI Layout

```
┌─────────────────────────────────────────────────────────┐
| ← Back  ↑ Up  🔄 Refresh  |  🔍 Search...  |  + New Folder │
├──────────┬──────────────────────────────────────────────┤
│ Sidebar  │  Files                                         │
│          │                                                │
│ 📁 Home  │  📁 Documents       🔒 encrypted              │
│ 📁 Docs  │  📁 Downloads       🌐 via Tor                │
│ 📁 DLs   │  📄 notes.txt                                 │
│ 📁 Music │  🖼️ photo.jpg        ⚠️ sensitive              │
│ 📁 Pics  │  📦 archive.zip                                │
│          │                                                │
│ 💾 USB   │                                                │
│ 🌐 Tor   │                                                │
├──────────┴──────────────────────────────────────────────┤
│ 12 items  |  Free space: 45.2 GB  |  Tor: 🟢 Active       │
└─────────────────────────────────────────────────────────┘
```

---

**Spec version:** 1.0
