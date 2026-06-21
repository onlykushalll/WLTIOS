# Spec: WLT-Update (Atomic Update System)

> **Status:** Draft (ready for implementation)
> **Phase:** 4 (Month 21)
> **Dependencies:** WLT-Init, WLT-Crypto, WLT-Pkg
> **ADR references:** None

## Purpose

WLT-Update handles OS updates using an A/B partition scheme. The system has two identical OS partitions. Updates download to the inactive partition while the OS runs on the active one. On reboot, the system switches to the updated partition. If boot fails, it auto-rolls back.

## How A/B Updates Work

```
Partition A: Current OS (active, running)
Partition B: Update target (inactive)

1. System runs on Partition A
2. WLT-Update downloads update to Partition B (while OS continues running)
3. WLT-Update verifies signature
4. On next reboot: boot from Partition B
5. If Partition B boots successfully: mark B as active
6. If Partition B fails to boot: auto-rollback to Partition A
```

**Zero downtime. Zero risk of bricking.**

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Update check | Repository (via Tor) | Version manifest |
| Update download | Repository (via Tor) | `.wlt-update.tar.zst` |
| Signature | Repository | Ed25519 signature |
| User command | CLI or WLT-Settings | `wltupdate check/install/rollback` |
| Current partition | Bootloader | Active partition (A or B) |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Updated partition | Inactive partition (A or B) | New OS files |
| Boot config | WLT-Boot config | Next boot target |
| Status | WLT-Settings | Update available / installed / failed |
| Rollback | Bootloader | Switch to previous partition |

## Dependencies

- **WLT-Boot** (bootloader reads partition config)
- **WLT-Crypto** (signature verification)
- **WLT-Pkg** (for package-level updates within a partition)
- **Tor** (all downloads via Tor)

## What Depends on This

- System security (updates patch vulnerabilities)
- User experience (seamless, no reboot interruptions)

## Error Handling

| Error | Action |
|-------|--------|
| Repository unreachable | Retry via Tor, then show "check failed" |
| Signature invalid | REFUSE update, show security warning |
| Download interrupted | Resume from where it left off |
| Disk full | Abort, show "need more space" |
| Boot fails after update | Auto-rollback to previous partition |
| Rollback fails | Drop to recovery mode |

## Tests

| Test | How |
|------|-----|
| Check for updates | Run `wltupdate check`, verify version info |
| Download update | Run `wltupdate install`, verify download completes |
| Boot to new partition | Reboot, verify new version active |
| Rollback | Run `wltupdate rollback`, reboot, verify old version |
| Auto-rollback | Corrupt new partition, reboot, verify auto-rollback |
| Signature check | Tamper with update, verify refusal |

## Security

- **Signed updates** — Ed25519 signature required
- **Tor download** — No IP leak during update check/download
- **Verified boot** — WLT-Boot checks partition signature before booting
- **No downgrades** — Rollback only to immediately-previous version (prevents downgrade attacks)
- **Atomic** — Update either fully completes or fully rolls back

## Performance

| Metric | Budget |
|--------|--------|
| Update check | < 5 seconds |
| Update download (500MB) | < 10 minutes (Tor-dependent) |
| Update install (copy to partition) | < 2 minutes |
| Reboot to new partition | < 3 seconds (same as normal boot) |
| Rollback | < 3 seconds (just boot other partition) |

## File Structure

```
src/userspace/wlt-update/
├── Cargo.toml
├── src/
│   ├── main.rs            # CLI entry
│   ├── check.rs           # Check for updates
│   ├── download.rs        # Download via Tor
│   ├── verify.rs          # Signature verification
│   ├── install.rs         # Install to inactive partition
│   ├── partition.rs       # Partition management (A/B)
│   ├── boot_config.rs     # Update bootloader config
│   ├── rollback.rs        # Rollback logic
│   └── error.rs
└── tests/
```

## CLI Commands

```bash
wltupdate check              # Check for updates
wltupdate install            # Download + install to inactive partition
wltupdate status             # Show update status
wltupdate rollback           # Rollback to previous partition
wltupdate enable-auto        # Enable automatic updates
wltupdate disable-auto       # Disable automatic updates
wltupdate history            # Show update history
```

## Partition Layout

```
/dev/sda1 (ESP, unencrypted)     — Bootloader + boot config
/dev/sda2 (LUKS2, Partition A)   — OS version 1 (active)
/dev/sda3 (LUKS2, Partition B)   — OS version 2 (inactive / update target)
/dev/sda4 (LUKS2, User data)     — User files (shared between A and B)
```

## Boot Config (`/boot/wltios.conf`)

```ini
[wlt-boot]
version = 1
timeout = 3
default = A                # Active partition (A or B)
rollback_on_fail = yes     # Auto-rollback if boot fails

[A]
kernel = /boot/sel4-a.bin
linux = /boot/linux-a.img
signature = /boot/sel4-a.sig
verify = yes
last_boot_success = 2026-06-21T10:00:00Z

[B]
kernel = /boot/sel4-b.bin
linux = /boot/linux-b.img
signature = /boot/sel4-b.sig
verify = yes
last_boot_success = 2026-06-20T10:00:00Z
```

## Update Flow

```
1. wltupdate check
   → Connect to repository via Tor
   → Fetch latest version manifest
   → Compare with current version
   → Show "update available" or "up to date"

2. wltupdate install
   → Download update package via Tor
   → Verify signature
   → Identify inactive partition (B if running on A)
   → Format inactive partition
   → Extract update to inactive partition
   → Update boot config: default = B
   → Mark "pending reboot"
   → User reboots

3. On reboot
   → WLT-Boot reads config, boots from B
   → If B boots successfully:
      → WLT-Boot updates last_boot_success for B
      → OS runs on B
      → A becomes inactive (rollback target)
   → If B fails to boot (3 attempts):
      → WLT-Boot auto-rolls back to A
      → Marks B as failed
      → User sees "update failed, rolled back" notification

4. wltupdate rollback
   → Swap boot config: default = A (or B, whichever is previous)
   → User reboots
   → System boots from previous partition
```

---

**Spec version:** 1.0
