# Spec: WLT-Pkg (Package Manager)

> **Status:** Draft (ready for implementation)
> **Phase:** 2 (Month 10)
> **Dependencies:** WLT-Init, WLT-Crypto
> **ADR references:** ADR-06 (musl libc)

## Purpose

WLT-Pkg is the package manager for WLTIOS. It installs, removes, and updates software packages atomically — meaning every operation either fully succeeds or fully fails, with no half-states.

## Design Philosophy

**Nix-style:** Every package version lives in its own directory (`/nix/store/wlt-<hash>-<name>-<version>/`). The system creates symlinks from standard paths (`/usr/bin/`) to the store. This makes:
- **Rollback trivial** — switch symlinks back to old version
- **No dependency hell** — each package has its own copy of dependencies
- **Reproducible** — same input always produces same store path
- **Atomic** — symlink swap is instant and atomic

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Package files | Repository (HTTPS via Tor) | `.wlt.tar.zst` (compressed tarball) |
| Package metadata | Repository | JSON manifest |
| User commands | CLI (`wltpkg install firefox`) | Command + args |
| Installed packages | `/var/wlt/store/` | Symlink tree |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Installed files | `/var/wlt/store/wlt-<hash>-<name>-<version>/` | Directory tree |
| Symlinks | `/usr/bin/`, `/usr/lib/`, etc. | Symlinks to store |
| Package database | `/var/wlt/db/packages.json` | JSON |
| Transaction log | `/var/wlt/log/transactions.log` | Text |

## Dependencies

- **WLT-Crypto** (package signature verification)
- **Tor** (all downloads via Tor — no direct HTTPS)
- **Rust crates:** serde, ring (crypto), reqwest (HTTP via Tor SOCKS), zstd (compression)

## What Depends on This

- All app installations
- System updates
- User experience (installing software is fundamental)

## Error Handling

| Error | Action |
|-------|--------|
| Repository unreachable | Try fallback mirror (via Tor), then fail with message |
| Package signature invalid | REFUSE to install — print warning |
| Package corrupt (hash mismatch) | Re-download, then refuse |
| Disk full | Rollback transaction, print error |
| Dependency conflict | Print conflict, refuse to install |
| Network timeout | Retry 3 times, then fail |
| Already installed (same version) | Print "already installed", no-op |

## Tests

| Test | How |
|------|-----|
| Install package | `wltpkg install <pkg>`, verify files appear |
| Remove package | `wltpkg remove <pkg>`, verify files gone |
| Update package | `wltpkg update <pkg>`, verify new version |
| Rollback | Install old version, update, rollback, verify old works |
| Atomic failure | Simulate disk full mid-install, verify no partial state |
| Signature check | Tamper with package, verify refusal |
| Offline install | Install from cache, verify works without network |
| Dependency resolution | Install package with deps, verify deps installed too |

## Security

- **All packages signed** — Ed25519 signature verified before install
- **Repository pinned** — Only official repo + user-added repos (with warning)
- **Downloads via Tor** — No direct HTTPS (prevents IP leak)
- **No execute during install** — Package scripts run in sandbox, not root
- **Hash verification** — Every file hash-checked against manifest
- **Sandboxed build** — Packages built reproducibly in isolated environment

## Performance

| Metric | Budget |
|--------|--------|
| Install small package | < 5 seconds |
| Install large package (100MB) | < 30 seconds |
| Remove package | < 2 seconds |
| Update package | < 10 seconds |
| Rollback | < 1 second (just symlink swap) |
| WLT-Pkg RAM | < 50MB |
| Binary size | < 2MB |

## Implementation Notes

- **Language:** Rust (memory safety, serde for JSON, ring for crypto)
- **Compression:** zstd (fast + good ratio)
- **Storage:** `/var/wlt/store/` (read-only after install)
- **Symlinks:** `/usr/bin/`, `/usr/lib/`, `/usr/share/` → store paths
- **Database:** JSON file (simple, no SQLite dependency)

## File Structure

```
src/userspace/wlt-pkg/
├── Cargo.toml
├── src/
│   ├── main.rs            # CLI entry point
│   ├── store.rs           # Package store management
│   ├── transaction.rs     # Atomic transaction logic
│   ├── dependency.rs      # Dependency resolution
│   ├── download.rs        # Download via Tor SOCKS proxy
│   ├── verify.rs          # Signature + hash verification
│   ├── database.rs        # Package database (JSON)
│   ├── symlink.rs         # Symlink management
│   ├── config.rs          # Config parsing
│   └── error.rs           # Error types
└── tests/
    ├── store_test.rs
    ├── transaction_test.rs
    └── dependency_test.rs
```

## CLI Commands

```bash
# Install
wltpkg install <package>           # Install latest version
wltpkg install <package>@<version> # Install specific version
wltpkg install <package-file>      # Install from local .wlt.tar.zst

# Remove
wltpkg remove <package>            # Remove package
wltpkg remove --orphans            # Remove unused dependencies

# Update
wltpkg update                      # Update all packages
wltpkg update <package>            # Update specific package
wltpkg update --check              # Check for updates (don't install)

# Rollback
wltpkg rollback <package>          # Rollback to previous version
wltpkg rollback --all              # Rollback all packages to last known good

# Query
wltpkg list                        # List installed packages
wltpkg info <package>              # Show package info
wltpkg search <query>              # Search repository
wltpkg files <package>             # List files owned by package

# Repository
wltpkg repo list                   # List configured repositories
wltpkg repo add <url>              # Add repository
wltpkg repo remove <name>          # Remove repository

# Cache
wltpkg cache clean                 # Clean download cache
wltpkg cache list                  # List cached packages
```

## Package Format

```
firefox-120.0.wlt.tar.zst
├── manifest.json          # Package metadata + file hashes
├── signature.sig          # Ed25519 signature of manifest
├── files/                 # Actual files
│   ├── usr/
│   │   ├── bin/
│   │   │   └── firefox
│   │   └── lib/
│   │       └── firefox/
│   │           └── ...
│   └── ...
└── hooks/                 # Post-install scripts (sandboxed)
    └── post-install.sh
```

### manifest.json

```json
{
  "name": "firefox",
  "version": "120.0",
  "description": "Privacy-focused web browser",
  "homepage": "https://www.mozilla.org/firefox",
  "license": "MPL-2.0",
  "architecture": "x86_64",
  "dependencies": [
    "gtk3@>=3.24",
    "alsa-lib@>=1.2",
    "dbus@>=1.12"
  ],
  "size_bytes": 78643200,
  "file_count": 1247,
  "files": [
    {"path": "usr/bin/firefox", "hash": "sha256:abc123..."},
    {"path": "usr/lib/firefox/libxul.so", "hash": "sha256:def456..."}
  ],
  "hooks": {
    "post_install": "hooks/post-install.sh",
    "pre_uninstall": "hooks/pre-uninstall.sh"
  }
}
```

## Atomic Transaction Flow

```
1. User runs: wltpkg install firefox
2. WLT-Pkg downloads manifest + signature (via Tor)
3. WLT-Pkg verifies signature
4. WLT-Pkg resolves dependencies
5. WLT-Pkg downloads all packages (via Tor)
6. WLT-Pkg verifies all hashes
7. WLT-Pkg begins transaction:
   a. Create new store directory: /var/wlt/store/wlt-<hash>-firefox-120.0/
   b. Extract files to store directory
   c. Verify all extracted file hashes
   d. Create symlinks in /usr/bin/ → store
   e. Update package database
   f. Run post-install hook (sandboxed)
8. If ANY step fails:
   a. Remove new store directory
   b. Remove new symlinks
   c. Restore old symlinks (if updating)
   d. Restore old database
   e. Print error, exit
9. If ALL steps succeed:
   a. Print success
   b. Log transaction
```

## Store Layout

```
/var/wlt/
├── store/                              # Package store (read-only)
│   ├── wlt-abc123...-firefox-120.0/
│   │   ├── usr/bin/firefox
│   │   └── usr/lib/firefox/...
│   ├── wlt-def456...-gtk3-3.24.0/
│   │   └── usr/lib/libgtk-3.so.0
│   └── ...
├── db/
│   ├── packages.json                   # Installed packages database
│   └── transactions.log                # Transaction history
├── cache/                              # Download cache
│   ├── firefox-120.0.wlt.tar.zst
│   └── ...
└── repos/                              # Repository metadata cache
    ├── official.json
    └── ...
```

## Symlink Tree

```
/usr/bin/
├── firefox → /var/wlt/store/wlt-abc123...-firefox-120.0/usr/bin/firefox
├── wltpkg → /var/wlt/store/wlt-789xyz...-wlt-pkg-1.0/usr/bin/wltpkg
└── ...

/usr/lib/
├── libgtk-3.so.0 → /var/wlt/store/wlt-def456...-gtk3-3.24.0/usr/lib/libgtk-3.so.0
└── ...
```

---

**Spec version:** 1.0
**Ready for implementation:** Yes (after WLT-Init is working)
