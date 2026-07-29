# WLTIOS — Networking and Tor (Consolidated Reference)

> **Supersedes:** `docs/specs/wlt-net.md`, `docs/specs/wlt-tor-mgr.md`,
> `docs/specs/wlt-tor-pd.md`, `docs/specs/firewall-pd.md`. Archived under
> `_ARCHIVED_2026-07-29`.

---

## 1. Where Tor-only enforcement actually happens today

**Not at the seL4 level.** Firewall PD (the spec'd component that would sit
in the network data path by construction, so Linux has no direct channel to
the NIC at all) does not exist. All enforcement today is Linux userspace:
route manipulation and, where available, iptables. See
`docs/ARCHITECTURE.md` §1 and §3 for why (no unified seL4+desktop track
yet).

## 2. Kill switch — real, independently corroborated

Route-based fallback (`SIOCDELRT` to delete the default gateway route,
`SIOCADDRT` to restore it), built specifically because the 64-bit guest's
Debian kernel has `CONFIG_IP_NF_IPTABLES=m` but the `.ko` isn't bundled and
can't be fetched (CDN too slow within the build environment's time budget).
Verified via QEMU serial output showing route deletion and restoration
(`PHASE5_FINAL_STATUS_ARCHIVED_2026-07-29.md`, 2026-07-XX session). The
32-bit guest instead has real iptables support (module built-in) as a
second, independent path. `wlt-net`'s `kill_switch.rs`-equivalent logic and
`wlt-shell`'s `cmd_killswitch` both reference this same mechanism - per the
Guardian handoff doc, Guardian's own kill-switch enforcement action reuses
the shell's existing implementation rather than duplicating it.

## 3. Tor bootstrap

Both guests report Tor bootstrapping to 100% and completing an actual
SOCKS5 round-trip (`torcheck` returning a real HTTP response through the
Tor SOCKS proxy). The 64-bit guest required swapping an Alpine-musl-linked
Tor binary for a glibc-linked one (0.4.9.6) after a Zlib version mismatch
caused `SIGBUS` on startup - documented with the specific root cause and
fix in the (archived) Phase 5 status report and independently plausible
given the specific version numbers cited.

## 4. TorPD (Tor isolated in a second seL4-managed VM)

See `docs/ARCHITECTURE.md` §3 and `docs/SECURITY.md` §1 for the current,
carefully-hedged status: the two-VM boot itself was fixed 2026-07-25
(`9897506`, a heap_size sizing bug root-caused against a working seL4
reference example) and reaches Buildroot login, but the actual SOCKS-relay
isolation behavior (Phase 2 of the original TorPD spec) and the vm0-vm1 IPC
proof were **not independently re-confirmed in this consolidation pass**.
Do not treat TorPD as "Tor is now isolated" without re-running the proof.

## 5. wlt-tor-mgr

Confirmed real by the orphaned-code audit: 2,060 lines, genuine Tor
control-port protocol implementation (`AUTHENTICATE`, `GETINFO`,
`SIGNAL NEWNYM` for new-identity, circuit-status parsing) - not a stub.
This is the component the desktop's Tor-status tray icon and "New Identity"
action are built on.

## 6. wlt-net

Original spec calls for Rust (tokio async runtime, neli for netlink). The
actual implementation is C, per the language table in
`docs/ARCHITECTURE.md` §5 - this mismatch is called out in the project's
own historical `MODULE_MAP.md`'s "Known Issues" list and remains
unresolved (not a new finding of this consolidation, just carried forward
accurately). WiFi scanning/connection via `wpa_supplicant`, MAC
randomization, and Tor-SOCKS-proxy forwarding are the spec'd
responsibilities; actual current line-by-line status of this specific
module was not part of the audits read for this consolidation and should
be independently re-checked before quoting a completeness percentage.

## 7. DNS leak prevention

Spec'd as "no system DNS resolver, all DNS through Tor." Guardian's
network-IDS layer (`net_engine.c`) includes rules specifically for non-Tor
UDP/53 traffic as an instant kill-switch trigger (per
`docs/AI_KAIROS.md` §3.2 and the Guardian research brief) - this is a
second, independent layer of DNS-leak defense beyond whatever `wlt-net`
itself enforces. Neither layer's DNS-leak behavior was re-tested in this
pass.

## 8. Open items

- Firewall PD (seL4-level Tor-only enforcement) - not started
- TorPD Phase 2 (SOCKS relay) + IPC proof - needs re-verification
- `wlt-net`'s Rust-vs-C gap and actual feature completeness - needs a
  dedicated re-audit (not covered by any audit read during this pass)
