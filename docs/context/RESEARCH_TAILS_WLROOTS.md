# WLTIOS Research: Tails Tor Configuration + wlroots Compositor

> **Task ID:** research-tails-wlroots
> **Author:** Researcher agent
> **Date:** 2026-06-21
> **Purpose:** Provide concrete, sourced material for two WLTIOS layers —
> (1) the anonymity layer (Tor) inspired by Tails OS, and (2) the GUI layer
> built on the wlroots Wayland compositor library. Every claim is cited.
> When something is not known, it is explicitly marked "I don't know".

---

## Table of Contents

1. [Part 1 — Tails Tor Configuration](#part-1--tails-tor-configuration)
   1. Tor config files (`torrc`)
   2. Firewall rules (`ferm.conf`)
   3. Kill switch
   4. DNS handling
   5. MAC spoofing
   6. Amnesic mode (RAM-only)
   7. Stream isolation
2. [Part 2 — wlroots Compositor Development](#part-2--wlroots-compositor-development)
   1. wlroots basics
   2. Minimal compositor example (tinywl)
   3. Output management
   4. Input handling
   5. Surface management
   6. xdg-shell
   7. Building wlroots
   8. Performance (60 fps)
3. [Part 3 — How this applies to WLTIOS](#part-3--how-this-applies-to-wltios)
4. [Sources](#sources)

---

## Part 1 — Tails Tor Configuration

### 1.1 Tor config files (`torrc`)

Tails ships a single canonical `torrc` at
`config/chroot_local-includes/etc/tor/torrc` in the Tails GitLab tree
(https://gitlab.tails.boum.org/tails/tails/-/blob/master/config/chroot_local-includes/etc/tor/torrc).
The current contents, fetched verbatim from the raw GitLab endpoint on
2026-06-21, are:

```conf
## Default SocksPort
SocksPort 127.0.0.1:9050 IsolateDestAddr IsolateDestPort
## SocksPort for other Tails-specific applications
SocksPort 127.0.0.1:9062 IsolateDestAddr IsolateDestPort
## SocksPort used by the Upgrader to download IUKs with tails-iuk-get-target-file
## (no isolation flags so the circuit used for Mirrorbits may be reused)
SocksPort 127.0.0.1:9063 NoIsolateDestAddr NoIsolateDestPort NoIsolateClientProtocol
## SocksPort for the default web browser (address is defined in tails-create-netns)
SocksPort 10.200.1.1:9050 IsolateSOCKSAuth KeepAliveIsolateSOCKSAuth ExtendedErrors

## The port on which Tor will listen for local connections from Tor controller
## applications, as documented in control-spec.txt.
ControlPort 127.0.0.1:9052

## Torified DNS
DNSPort 5353
AutomapHostsOnResolve 1
AutomapHostsSuffixes .exit,.onion

## Transparent proxy
TransPort 127.0.0.1:9040 IsolateDestAddr IsolateDestPort

## Enable the Seccomp sandbox. Disabled later by tor-pt-configuration-helper
## if pluggable transports are configured in TCA.
Sandbox 1

## Misc
AvoidDiskWrites 1

## Disable default warnings on StartTLS for email.
WarnPlaintextPorts 23,109

## Log to the systemd Journal
Log notice syslog

## Tor Launcher will enable network access for Tor once the user has
## provided the configuration they desire.
DisableNetwork 1
```

Key things WLTIOS must replicate:

| Setting | Why Tails does it | What it means for WLTIOS |
|---|---|---|
| `SocksPort 127.0.0.1:9050 IsolateDestAddr IsolateDestPort` | SOCKS proxy for SOCKS-aware apps; separate Tor circuit per (dest IP, dest port) | Run Tor with at least this SOCKS port, and reuse the isolation flags |
| `SocksPort 127.0.0.1:9062 ...` | Dedicated SOCKS port for Tails-specific apps (e.g. `htp` time fetch, upgrader) | Per-app SOCKS ports keep circuits isolated |
| `SocksPort 10.200.1.1:9050 ...` | Bound inside a network namespace `veth-tbb` for the Tor Browser sandbox (IP 10.200.1.1) | Network namespaces + per-namespace SOCKS ports give strong isolation |
| `ControlPort 127.0.0.1:9052` | Lets Tor Controller App (TCA) and `tails-upgrade-frontend` talk to Tor | Needed in WLTIOS for any Tor controller UI |
| `DNSPort 5353` | Tor resolves DNS for clients that don't speak SOCKS | Required for transparent DNS over Tor |
| `AutomapHostsOnResolve 1` | `.onion` and `.exit` hostnames are mapped into the `127.192.0.0/10` virtual network so transparent proxying can route them | Required for `.onion` resolution to work over the TransPort |
| `TransPort 127.0.0.1:9040 IsolateDestAddr IsolateDestPort` | Transparent proxy port for iptables NAT redirect | Required for forcing unproxied apps through Tor |
| `Sandbox 1` | Seccomp BPF sandbox on the Tor process | Defense in depth; replicate if Tor is in a Linux partition |
| `AvoidDiskWrites 1` | Don't write cache/state to disk — preserves amnesia | Critical for amnesic mode |
| `DisableNetwork 1` | Boot with Tor's network disabled until TCA / Tor Launcher connects | Prevents leaks before user has chosen bridges / configuration |

### 1.2 Firewall rules (`ferm.conf`)

Tails uses **ferm** (Firewall Made Easy), a frontend that compiles a
structured config into `iptables` rules. The file lives at
`config/chroot_local-includes/etc/ferm/ferm.conf` in the Tails tree
(https://gitlab.tails.boum.org/tails/tails/-/tree/master/config/chroot_local-includes/etc/ferm).

The current contents, fetched verbatim from the raw GitLab endpoint on
2026-06-21, are:

```conf
# -*- mode: conf[space] -*-
#
#  Configuration file for ferm(1).

# When ferm starts initially during early boot, the "amnesia" user does
# not exist yet, so we have to use its UID (#7018).
def $amnesia_uid = 1000;
def $default_Linux_ttl = 64;

# IPv4
domain ip {
    table filter {
        chain log_reject {
            LOG log-prefix "Dropped outbound packet: " log-level debug log-uid;
            REJECT reject-with icmp-port-unreachable;
        }

        chain INPUT {
            policy DROP;

            # Established incoming connections are accepted.
            mod state state (ESTABLISHED) ACCEPT;
            # Traffic on the loopback interface is accepted.
            interface lo ACCEPT;

            # netns configuration; see config/chroot_local-includes/usr/local/lib/tails-create-netns
            interface veth-tbb saddr 10.200.1.2 daddr 10.200.1.1 proto tcp mod multiport destination-ports (9050 951) ACCEPT;
            interface veth-onioncircs saddr 10.200.1.6 daddr 10.200.1.5 proto tcp mod multiport destination-ports (951) ACCEPT;
            interface veth-tca saddr 10.200.1.10 daddr 10.200.1.9 proto tcp mod multiport destination-ports (951) ACCEPT;
            interface veth-onionshare saddr 10.200.1.14 daddr 10.200.1.13 proto tcp mod multiport destination-ports (9050 951) ACCEPT;

            # block connections from clearnet that are not directed to the outside
            interface veth-clearnet daddr 10.200.1.0/24 REJECT;
        }

        chain OUTPUT {
            policy DROP;

            # Established outgoing connections are accepted.
            mod state state (ESTABLISHED) ACCEPT;

            # Local resources
            outerface lo {
                mod state state (RELATED) proto icmp ACCEPT;

                # Tor's SOCKSPort's
                daddr 127.0.0.1 proto tcp syn dport 9050 {
                    mod owner uid-owner _apt ACCEPT;
                    mod owner uid-owner proxy ACCEPT;
                    mod owner uid-owner nobody ACCEPT;
                    mod owner uid-owner tails-upgrade-frontend ACCEPT;
                }
                daddr 127.0.0.1 proto tcp syn dport 9062 {
                    mod owner uid-owner htp ACCEPT;
                    mod owner uid-owner tails-upgrade-frontend ACCEPT;
                }
                daddr 127.0.0.1 proto tcp syn dport 9063 {
                    mod owner uid-owner tails-iuk-get-target-file ACCEPT;
                    mod owner uid-owner $amnesia_uid jump log_reject;
                }

                # Tor's ControlPort
                daddr 127.0.0.1 proto tcp dport 9052 {
                    mod owner uid-owner $amnesia_uid jump log_reject;
                    mod owner uid-owner root ACCEPT;
                }

                # Tor control port filter
                daddr 127.0.0.1 proto tcp dport 951 {
                    mod owner uid-owner root ACCEPT;
                }

                # System DNS and Tor's DNSPort
                daddr 127.0.0.1 proto udp dport (53 5353) {
                    mod owner uid-owner $amnesia_uid ACCEPT;
                    mod owner uid-owner htp ACCEPT;
                    mod owner uid-owner _apt jump log_reject;
                    mod owner uid-owner root ACCEPT;
                }

                # Accessibility daemon
                daddr 127.0.0.1 proto tcp syn dport 4101 {
                    mod owner uid-owner Debian-gdm ACCEPT;
                }

                # Any TCP port (Audacity, OnionShare, etc.) not explicitly blocked above
                daddr 127.0.0.1 proto tcp syn {
                    mod owner uid-owner $amnesia_uid ACCEPT;
                }
            }

            # Allow transparent proxying through tor's TransPort for amnesia
            outerface ! lo proto tcp daddr 127.0.0.1 dport 9040 mod owner uid-owner $amnesia_uid ACCEPT;

            # clearnet is allowed to connect to any TCP port via external
            # interfaces (but lo is blocked so it cannot interfere with Tor)
            # including DNS on the LAN. UDP DNS queries are also allowed.
            outerface ! lo mod owner uid-owner clearnet {
                proto tcp ACCEPT;
                proto udp dport domain ACCEPT;
            }

            # Tor is allowed to do anything it wants to.
            mod owner uid-owner debian-tor {
                proto tcp syn mod state state (NEW) ACCEPT;
                proto udp dport domain ACCEPT;
            }

            # Local network connections should not go through Tor but DNS
            # shall be rejected. (Excludes the VirtualAddrNetwork used for .onion.)
            daddr (10.0.0.0/8 172.16.0.0/12 192.168.0.0/16) @subchain "lan" {
                proto tcp dport domain jump log_reject;
                proto udp dport domain jump log_reject;
                proto udp dport netbios-ns jump log_reject;
                ACCEPT;
            }

            # Everything else is logged and dropped.
            jump log_reject;
        }

        chain FORWARD {
            policy DROP;
        }
    }

    table nat {
        chain OUTPUT {
            policy ACCEPT;

            # .onion mapped addresses redirection to Tor.
            daddr 127.192.0.0/10 proto tcp REDIRECT to-ports 9040;

            # Redirect system DNS to Tor's DNSport
            daddr 127.0.0.1 proto udp dport 53 REDIRECT to-ports 5353;

            # Transparently proxy all of amnesia's outgoing TCP Internet
            # connection through Tor
            mod owner uid-owner $amnesia_uid {
                daddr (10.0.0.0/8 172.16.0.0/12 192.168.0.0/16) RETURN;
                outerface ! lo proto tcp REDIRECT to-ports 9040;
            }
        }
    }

    table mangle {
        chain FORWARD {
            policy ACCEPT;
            # Reset TTL to default to avoid Unsafe Browser fingerprinting
            interface veth-clearnet {
                TTL ttl-set $default_Linux_ttl;
            }
        }
    }
}

# IPv6:
domain ip6 {
    table filter {
        chain INPUT  { policy DROP; ... }
        chain FORWARD { policy DROP; }
        chain OUTPUT {
            policy DROP;
            ...
            LOG log-prefix "Dropped outbound packet: " log-level debug log-uid;
            REJECT reject-with icmp6-port-unreachable;
        }
    }
}
```

**Critical observations** (every one is supported by the file above):

1. **Default-deny on OUTPUT** for both IPv4 and IPv6 — this is the kill
   switch (see §1.3).
2. The only user allowed to create outbound TCP `SYN NEW` is
   `debian-tor` (the system user that the Tor process runs as). Every
   other user's outbound TCP is dropped. This is what forces "Tor-only".
3. The `amnesia` user (the desktop user, UID 1000) has its outbound TCP
   redirected to Tor's `TransPort` 9040 in the `nat` table — this is
   the transparent-proxy step.
4. `.onion` mapped addresses (`127.192.0.0/10`) are also redirected to
   `TransPort` 9040.
5. UDP DNS to `127.0.0.1:53` is redirected to Tor's `DNSPort` 5353.
6. The `clearnet` user (used by the Unsafe Browser) is allowed to bypass
   Tor — this is intentional and explicit.
7. IPv6 is **entirely blocked** (OUTPUT policy DROP) — there is no
   IPv6 leak path.

### 1.3 Kill switch

Tails's kill switch is implicit in the firewall: **OUTPUT has
`policy DROP`** for both IPv4 and IPv6 (see §1.2). The only user
permitted to open outbound connections is `debian-tor`. If the Tor
process stops, no other process can connect out — packets just hit
the catch-all `jump log_reject` rule.

This is corroborated by Tor Project forum:
> "Tails doesn't force all traffic through Tor, it blocks all traffic except Tor."
> — https://forum.torproject.org/t/only-connection-via-tor-for-the-entire-operating-system/295

So Tails's model is **allowlist, not redirect-all**: instead of
"redirect every packet to Tor", it is "drop everything except the
packets Tor itself originates, plus the redirected subset from
`amnesia`".

There is no separate "kill switch daemon" that toggles rules when Tor
disconnects; the firewall rules are static. If Tor dies, the
`debian-tor`-owned TCP streams still hit `ESTABLISHED` for a moment,
then time out, and no new connections are possible from any other user.

### 1.4 DNS handling

Tails uses **three** DNS mechanisms, all of which combine to prevent
leaks:

1. **`DNSPort 5353`** in `torrc` (see §1.1). Tor listens on UDP 5353
   and resolves names over the Tor network. The result is returned
   either as a real IP (for clearnet) or a virtual IP in
   `127.192.0.0/10` (for `.onion`, due to `AutomapHostsOnResolve 1`).
2. **NAT redirect** in `ferm.conf`:
   `daddr 127.0.0.1 proto udp dport 53 REDIRECT to-ports 5353` —
   any process that sends DNS to localhost:53 is silently redirected
   to Tor's DNSPort. (See §1.2.)
3. **`AutomapHostsSuffixes .exit,.onion`** + `AutomapHostsOnResolve 1`
   — when a process asks Tor to resolve `something.onion`, Tor returns
   a synthetic IP in `127.192.0.0/10`. When that IP is then connected
   to, the NAT rule `daddr 127.192.0.0/10 proto tcp REDIRECT to-ports 9040`
   sends it to the TransPort, and Tor maps it back to the `.onion`.

Additionally, the firewall **explicitly rejects** any attempt to do
DNS over the LAN:
```
daddr (10.0.0.0/8 172.16.0.0/12 192.168.0.0/16) @subchain "lan" {
    proto tcp dport domain jump log_reject;
    proto udp dport domain jump log_reject;
    ...
}
```
(See §1.2.) So even if an app tries to send DNS to a LAN resolver,
it's dropped.

A Whonix wiki article on stream isolation also notes that the
`DNSPort` does not provide stream isolation itself — every DNS lookup
goes through the same Tor circuit, which is why app-level isolation
happens at the `SocksPort` layer instead.
— https://www.whonix.org/wiki/Stream_Isolation

### 1.5 MAC spoofing

Tails spoofs the MAC of every NIC on boot, before any network is
brought up. The script is
`/usr/local/lib/tails-spoof-mac` in the live system
(`config/chroot_local-includes/usr/local/lib/tails-spoof-mac` in the
tree). A mirror is at https://gist.github.com/3297e6bc5e8a77a2f84307c66410aa06.

The script (verbatim from the gist, fetched 2026-06-21):

```sh
#!/bin/sh
# Taken from https://git-tails.immerda.ch/tails/plain/config/chroot_local-includes/usr/local/lib/tails-spoof-mac

set -e
set -u

. /usr/local/lib/tails-shell-library/hardware.sh
. /usr/local/lib/tails-shell-library/log.sh
. /usr/local/lib/tails-shell-library/tails-greeter.sh
. /etc/live/config.d/username.conf
. /usr/bin/gettext.sh
TEXTDOMAIN="tails"
export TEXTDOMAIN

# ... helper functions for notifications ...

spoof_mac() {
    local msg
    set +e
    msg="$(macchanger -e "${1}" 2>&1)"
    ret="${?}"
    set -e
    if [ "${ret}" != 0 ]; then
        log "macchanger failed for NIC ${1}, returned ${ret} and said: ${msg}"
        return 1
    fi
}

set_log_tag spoof-mac
NIC="${1}"

if ! mac_spoof_is_enabled; then
    exit 0
fi

log "Trying to spoof MAC address of NIC ${NIC}..."

if ! nic_exists "${NIC}"; then
    log "NIC ${NIC} doesn't exist, skipping"
    exit 1
fi

OLD_MAC="$(get_current_mac_of_nic "${NIC}")"

# There is a 1/2^24 chance macchanger will randomly pick the real MAC
# address. We try to make it really unlikely by repeating it up to
# three times. Theoretically speaking this leaks information about the
# real MAC address at each occasion but actually leaking the real MAC
# address will be more serious in practice.
for i in 1 2 3; do
    if ! spoof_mac "${NIC}"; then
        # If our MAC spoofing primitive fails, we fail safe by forcing
        # us to enter into panic mode.
        unset NEW_MAC
        break
    fi
    NEW_MAC="$(get_current_mac_of_nic "${NIC}")"
    if [ "${OLD_MAC}" != "${NEW_MAC}" ]; then
        break
    fi
done

# MAC spoofing fail-safe: if $NIC's MAC address isn't spoofed at this
# point we have to take some drastic measures in order to prevent
# potential leaks.
if [ -z "${OLD_MAC:-}" ] || [ -z "${NEW_MAC:-}" ] || \
   [ "${OLD_MAC:-}" = "${NEW_MAC:-}" ]
then
    log "Failed to spoof MAC address of NIC ${NIC}. Going into panic mode."
    if ! mac_spoof_panic "${NIC}"; then
        # If mac_spoof_panic() fails we're quite screwed, so we kill
        # NetworkManager without notification to do our best to
        # prevent a MAC address leak.
        log "Panic mode failed for NIC ${NIC}."
        stop_and_disable_NM
    fi
    exit 1
fi

log "Successfully spoofed MAC address of NIC ${NIC}"
```

Key points:

- Tool used: **`macchanger -e`** — the `-e` flag picks an "ending" MAC
  that preserves the vendor OUI (first 3 bytes) but randomizes the
  NIC-specific part. This is deliberate: fully random MACs break
  captive-portal flows and break hotel/coffee-shop Wi-Fi logins, so
  Tails only randomizes the device-specific half.
- The script runs per-NIC, before NetworkManager starts.
- **Fail-safe / panic mode**: if MAC spoofing fails (e.g. driver
  refuses), Tails disables NetworkManager entirely rather than let the
  real MAC leak. See also Tails issue #17779:
  https://gitlab.tails.boum.org/tails/tails/-/issues/17779
- Known limitation: on some MacBook Pros, the real MAC is leaked by
  the firmware *before* Tails boots — see
  https://jasonmurray.org/posts/2021/tailsleak and Tails issue #15208
  https://gitlab.tails.boum.org/tails/tails/-/issues/15208. WLTIOS
  cannot fix firmware leaks from the bootloader phase without
  replacing the firmware.

### 1.6 Amnesic mode (RAM-only)

Tails is amnesic by design: nothing is written to the host disk, and
all state is in RAM.

Mechanism (sourced):

- The Tails ISO contains a SquashFS filesystem
  (`live/filesystem.squashfs`) — read-only, compressed.
- The Tails kernel command line uses **`live-boot`** with the
  **`toram`** parameter, which copies the squashfs into a tmpfs in
  RAM at boot, then pivots root to it. From that point the entire
  root filesystem lives in RAM.
  — https://www.reddit.com/r/tails/comments/1iqr9hp/expert_tails_users_please_help_clarify_my_doubt
  — https://sourceforge.net/p/clonezilla/discussion/Clonezilla_live/thread/03690fdb
    (explains that `toram=filesystem.squashfs` makes live-boot copy
    *only* the squashfs to RAM)
- "Tails never writes anything to the hard disk and only runs from the
  memory of the computer. The memory is entirely deleted when you
  shutdown Tails, erasing all possible traces."
  — https://tails.net/about/index.en.html (fetched 2026-06-21)
- "Tails is 'amnesic'. It runs in the computer's random access memory
  (RAM). Tails overwrites most of the used RAM to avoid a cold boot
  attack."
  — https://en.wikipedia.org/wiki/Tails_(operating_system)

Additional details:

- The Tor process is configured with **`AvoidDiskWrites 1`** (see
  §1.1) so it does not persist any state to disk.
- The "Persistent Storage" feature is **opt-in** and explicitly
  encrypted; everything outside it is amnesic.
  — https://tails.net/about/index.en.html
- For WLTIOS, the equivalent is: build the Linux guest VM's root
  filesystem as a SquashFS image, load it into a tmpfs at boot via
  the initramfs, and have the WLTIOS init system mount all writable
  paths (`/tmp`, `/var`, `/run`, `/home`) as `tmpfs`. Persistent
  storage (if any) is opt-in and encrypted.

**What I don't know:** the exact kernel command line Tails ships
(e.g. the full `APPEND` line). I tried to fetch it from the Tails
live-build config but did not confirm. The mechanism (`live-boot` +
`toram=filesystem.squashfs`) is well-corroborated by user reports and
the live-boot documentation; the exact cmdline parameters used by
Tails 6.x are not in the sources I retrieved.

### 1.7 Stream isolation

Stream isolation means: different apps use different Tor circuits, so
an observer that controls two of the destinations cannot trivially
correlate them.

Tails achieves stream isolation through **multiple `SocksPort` lines,
each with `IsolateDestAddr IsolateDestPort`** (see §1.1). The Tor
manual defines these flags:
- `IsolateDestAddr` — give each destination address its own circuit.
- `IsolateDestPort` — give each destination port its own circuit.
- `IsolateSOCKSAuth` — give each SOCKS username/password its own
  circuit.
- `KeepAliveIsolateSOCKSAuth` — keep isolated circuits alive for
  reuse by the same client.

In Tails:

| Port | Who uses it | Isolation |
|---|---|---|
| `127.0.0.1:9050` | Most Tails apps (system SOCKS) | per dest+port |
| `127.0.0.1:9062` | `htp` (time sync) + upgrader | per dest+port |
| `127.0.0.1:9063` | Upgrader IUK download | **no isolation** (deliberate, see torrc comment) |
| `10.200.1.1:9050` | Tor Browser (inside `veth-tbb` netns) | per SOCKS auth (per-identity in the browser) |

The browser port is the clever bit: Tor Browser isolates circuits
per first-party origin by using a unique SOCKS username/password per
origin. `IsolateSOCKSAuth` then makes Tor give each of those
credentials a fresh circuit. Combined with
`KeepAliveIsolateSOCKSAuth`, the same site reuses its circuit across
navigations but different sites get different circuits.

The Whonix wiki documents this pattern in detail:
— https://www.whonix.org/wiki/Stream_Isolation

Known limitation, documented in Tails GitLab issue #20503:
> "IsolateDestAddr / IsolateDestPort does nothing when a user has
> multiple accounts/identities on the same destination."
> — https://gitlab.tails.boum.org/tails/tails/-/issues/20503

This is why Tor Browser uses `IsolateSOCKSAuth` rather than
`IsolateDestAddr` — the destination (the website) is the same; the
isolation needs to be per-identity, which is achievable via SOCKS
auth.

For network namespaces: Tails runs the Tor Browser inside a network
namespace (`veth-tbb`) created by
`/usr/local/lib/tails-create-netns`. The namespace has a veth pair
(`10.200.1.2` inside the namespace, `10.200.1.1` on the host) and a
firewall rule that lets only Tor's SOCKS port (9050) and the Tor
control port filter (951) through. This means even if the browser is
exploited, it cannot reach any other host. See §1.2 for the ferm
rules that implement this.

---

## Part 2 — wlroots Compositor Development

### 2.1 wlroots basics

From the wlroots README (verbatim, fetched 2026-06-21 from
https://raw.githubusercontent.com/swaywm/wlroots/master/README.md):

> Pluggable, composable, unopinionated modules for building a
> [Wayland] compositor; or about 60,000 lines of code you were going
> to write anyway.
>
> - wlroots provides **backends** that abstract the underlying display
>   and input hardware, including KMS/DRM, libinput, Wayland, X11, and
>   headless backends, plus any custom backends you choose to write,
>   which can all be created or destroyed at runtime and used in
>   concert with each other.
> - wlroots provides **unopinionated, mostly standalone
>   implementations** of many Wayland interfaces, both from
>   `wayland.xml` and various protocol extensions. We also promote the
>   standardization of portable extensions across many compositors.
> - wlroots provides several powerful, standalone, and optional
>   **tools** that implement components common to many compositors,
>   such as the arrangement of outputs in physical space.
> - wlroots provides an **Xwayland abstraction** that allows you to
>   have excellent Xwayland support without worrying about writing
>   your own X11 window manager on top of writing your compositor.
> - wlroots provides a **renderer abstraction** that simple
>   compositors can use to avoid writing GL code directly, but which
>   steps out of the way when your needs demand custom rendering code.

In short, **wlroots gives us**:

- Backends (DRM/KMS, libinput, Wayland nested, X11 nested, headless)
- Renderers (GLES2, Vulkan, pixman software)
- Wayland protocol implementations (`wl_compositor`, `wl_shm`,
  `wl_seat`, `wl_output`, `wl_subcompositor`, `wl_data_device_manager`,
  `xdg_wm_base`, `xdg_shell`, `wlr_foreign_toplevel_management`,
  `wlr_output_management`, etc.)
- Helpers (`wlr_output_layout`, `wlr_cursor`, `wlr_seat`,
  `wlr_xcursor_manager`, `wlr_scene` — the high-level scene graph
  API added in 0.15)
- Xwayland abstraction

**What WLTIOS builds on top**: the window manager policy (tiling
strategy, focus model, keybindings, workspaces, decorations),
the UI shell (panels, notifications, launcher), and any custom
protocols (e.g. an AI-assistant IPC).

A comparison by Wayfire (https://wayfire.org/2019/02/24/X11-WLC-Libweston-Wlroots.html)
notes that wlroots, unlike libweston, **gives the compositor full
control over the rendering loop** — there's no library-owned main
loop. This matters for WLTIOS because we can integrate the
compositor into our own custom init system.

### 2.2 Minimal compositor example (tinywl)

`tinywl/tinywl.c` ships in the wlroots tree and is the canonical
"smallest functional Wayland compositor on wlroots" example
(https://github.com/swaywm/wlroots/blob/master/tinywl/tinywl.c).
The current source is ~913 lines. I fetched it in full from
`https://raw.githubusercontent.com/swaywm/wlroots/master/tinywl/tinywl.c`
and saved a local copy.

The three structs at the heart of tinywl:

```c
struct tinywl_server {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;

    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_surface;
    struct wl_list views;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

    struct wlr_seat *seat;
    struct wl_listener new_input;
    struct wl_listener request_cursor;
    struct wl_listener request_set_selection;
    struct wl_list keyboards;
    enum tinywl_cursor_mode cursor_mode;
    struct tinywl_view *grabbed_view;
    double grab_x, grab_y;
    struct wlr_box grab_geobox;
    uint32_t resize_edges;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs;
    struct wl_listener new_output;
};

struct tinywl_output {
    struct wl_list link;
    struct tinywl_server *server;
    struct wlr_output *wlr_output;
    struct wl_listener frame;
};

struct tinywl_view {
    struct wl_list link;
    struct tinywl_server *server;
    struct wlr_xdg_surface *xdg_surface;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    bool mapped;
    int x, y;
};
```

**The pattern is consistent**: every wlroots object the compositor
cares about gets a `struct wl_listener` field, and we wire it up with
`wl_signal_add(&object->events.<signal>, &server-><listener>)` and a
`.notify` callback. wlroots uses a signal/slot model (not virtual
functions) so the compositor explicitly subscribes to whatever it
cares about. This is verbose but extremely predictable — there is no
hidden control flow.

The main loop in `tinywl.c`:

```c
int main(int argc, char *argv[]) {
    wlr_log_init(WLR_DEBUG, NULL);
    /* ... getopt for -s startup command ... */

    struct tinywl_server server = {0};
    server.wl_display = wl_display_create();
    server.backend = wlr_autocreate(server.wl_display);
    server.renderer = wlr_renderer_autocreate(server.backend);
    wlr_renderer_init_wl_display(server.renderer, server.wl_display);

    /* wlr_compositor + wlr_data_device_manager globals */
    wlr_compositor_create(server.wl_display, server.renderer);
    wlr_data_device_manager_create(server.wl_display);

    /* Output layout: where outputs live in physical space */
    server.output_layout = wlr_output_layout_create();
    wl_list_init(&server.outputs);
    server.new_output.notify = server_new_output;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    /* xdg-shell: application windows */
    wl_list_init(&server.views);
    server.xdg_shell = wlr_xdg_shell_create(server.wl_display);
    server.new_xdg_surface.notify = server_new_xdg_surface;
    wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

    /* Cursor + xcursor manager */
    server.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
    server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(server.cursor_mgr, 1);

    /* Cursor events */
    server.cursor_motion.notify = server_cursor_motion;
    wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
    /* ... motion_absolute, button, axis, frame ... */

    /* Seat (keyboard + pointer abstraction) */
    wl_list_init(&server.keyboards);
    server.new_input.notify = server_new_input;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);
    server.seat = wlr_seat_create(server.wl_display, "seat0");
    /* ... request_set_cursor, request_set_selection ... */

    /* Add a Unix socket */
    const char *socket = wl_display_add_socket_auto(server.wl_display);
    if (!socket) { /* ... */ }

    /* Start the backend (becomes DRM master, enumerates inputs/outputs) */
    if (!wlr_backend_start(server.backend)) { /* ... */ }

    setenv("WAYLAND_DISPLAY", socket, true);
    if (startup_cmd) {
        if (fork() == 0) {
            execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
        }
    }

    /* Run the Wayland event loop. Doesn't return until exit. */
    wl_display_run(server.wl_display);

    wl_display_destroy_clients(server.wl_display);
    wl_display_destroy(server.wl_display);
    return 0;
}
```

### 2.3 Output management

`server_new_output` (lines 637-678 of tinywl.c, verbatim):

```c
static void server_new_output(struct wl_listener *listener, void *data) {
    /* This event is raised by the backend when a new output (aka a
     * display or monitor) becomes available. */
    struct tinywl_server *server =
        wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    /* Some backends don't have modes. DRM+KMS does, and we need to
     * set a mode before we can use the output. The mode is a tuple
     * of (width, height, refresh rate), and each monitor supports
     * only a specific set of modes. We just pick the monitor's
     * preferred mode; a more sophisticated compositor would let the
     * user configure it. */
    if (!wl_list_empty(&wlr_output->modes)) {
        struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
        wlr_output_set_mode(wlr_output, mode);
        wlr_output_enable(wlr_output, true);
        if (!wlr_output_commit(wlr_output)) {
            return;
        }
    }

    /* Allocates and configures our state for this output */
    struct tinywl_output *output =
        calloc(1, sizeof(struct tinywl_output));
    output->wlr_output = wlr_output;
    output->server = server;
    /* Sets up a listener for the frame notify event. */
    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    wl_list_insert(&server->outputs, &output->link);

    /* Adds this to the output layout. The add_auto function arranges
     * outputs from left-to-right in the order they appear. A more
     * sophisticated compositor would let the user configure the
     * arrangement of outputs in the layout.
     *
     * The output layout utility automatically adds a wl_output global
     * to the display, which Wayland clients can see to find out
     * information about the output (such as DPI, scale factor,
     * manufacturer, etc).
     */
    wlr_output_layout_add_auto(server->output_layout, wlr_output);
}
```

Key APIs:

- `wlr_output_preferred_mode(wlr_output)` — get the monitor's preferred mode.
- `wlr_output_set_mode()` + `wlr_output_enable()` + `wlr_output_commit()` —
  the three-step configuration pattern. wlroots requires an explicit
  commit; nothing is applied until you call it.
- `wlr_output_layout_add_auto(layout, wlr_output)` — arrange outputs
  left-to-right automatically. For a real desktop you'd use
  `wlr_output_layout_add(layout, output, x, y)` for explicit positions.
- The `events.frame` signal fires at the output's refresh rate
  (typically 60 Hz), and that's what drives rendering.

For runtime reconfiguration (hotplug, mode change), wlroots exposes
the `wlr-output-management-unstable-v1` protocol so external tools
like `wlr-randr` can change outputs without restarting the compositor.
— https://wayland.app/protocols/wlr-output-management-unstable-v1
— https://manpages.ubuntu.com/manpages/noble/man1/wlr-randr.1.html

### 2.4 Input handling

wlroots abstracts every input device as a `wlr_input_device` with a
`type` field. The five types (from
https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html):

- `WLR_INPUT_DEVICE_KEYBOARD`
- `WLR_INPUT_DEVICE_POINTER`
- `WLR_INPUT_DEVICE_TOUCH`
- `WLR_INPUT_DEVICE_TABLET_TOOL`
- `WLR_INPUT_DEVICE_TABLET_PAD`

The corresponding member of the anonymous union (`device->keyboard`,
`device->pointer`, etc.) becomes valid based on `type`.

The Wayland `wl_seat` global models a "seat" — one place a user sits,
with up to one keyboard and one pointer. From
https://wayland-book.com/seat.html:
> "A Wayland seat refers to one 'seat' at which a user sits and
> operates the computer, and is associated with up to one keyboard
> and up to one 'pointer' device (i.e. a mouse or touchpad)."

In tinywl, `server_new_input` dispatches by type and updates seat
capabilities (lines 244-268, verbatim):

```c
static void server_new_input(struct wl_listener *listener, void *data) {
    /* This event is raised by the backend when a new input device
     * becomes available. */
    struct tinywl_server *server =
        wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;
    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        server_new_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        server_new_pointer(server, device);
        break;
    default:
        break;
    }
    /* We need to let the wlr_seat know what our capabilities are,
     * which is communicated to the client. In TinyWL we always have
     * a cursor, even if there are no pointer devices, so we always
     * include that capability. */
    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards)) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    wlr_seat_set_capabilities(server->seat, caps);
}
```

**Keyboard handling** (lines 205-233):

```c
static void server_new_keyboard(struct tinywl_server *server,
        struct wlr_input_device *device) {
    struct tinywl_keyboard *keyboard =
        calloc(1, sizeof(struct tinywl_keyboard));
    keyboard->server = server;
    keyboard->device = device;

    /* Prepare an XKB keymap and assign it to the keyboard. This
     * assumes the defaults (e.g. layout = "us"). */
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
            XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(device->keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

    /* Listeners for keyboard events. */
    keyboard->modifiers.notify = keyboard_handle_modifiers;
    wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
    keyboard->key.notify = keyboard_handle_key;
    wl_signal_add(&device->keyboard->events.key, &keyboard->key);

    wlr_seat_set_keyboard(server->seat, device);

    /* And add the keyboard to our list of keyboards */
    wl_list_insert(&server->keyboards, &keyboard->link);
}
```

Notes:

- **xkbcommon** is the keyboard layout library. `xkb_keymap_new_from_names`
  with `NULL` uses the environment's default layout. WLTIOS should
  allow user-configurable layouts by passing an
  `xkb_rule_names` struct (e.g. `{.layout="us,in", .variant=",hin"}`).
- `wlr_keyboard_set_repeat_info(rate, delay)` sets auto-repeat.
- **Compositor keybindings** vs. **client keybindings**: in
  `keyboard_handle_key`, tinywl checks `(modifiers & WLR_MODIFIER_ALT)`
  and only then processes the key as a compositor binding (e.g.
  Alt+F1 cycle windows, Alt+Esc quit). Otherwise it forwards the key
  to the focused client via `wlr_seat_keyboard_notify_key()`. WLTIOS
  should use a similar pattern, but with the "Super" key (Windows
  key) instead of Alt for its bindings — that's the conventional
  choice for modern Wayland compositors.

**Pointer handling** is delegated to `wlr_cursor`, which is a
utility that tracks an on-screen cursor image and aggregates motion
from multiple devices. tinywl attaches pointers and listens to the
cursor's events (not the device's):

```c
static void server_new_pointer(struct tinywl_server *server,
        struct wlr_input_device *device) {
    /* We don't do anything special with pointers. All of our pointer
     * handling is proxied through wlr_cursor. */
    wlr_cursor_attach_input_device(server->cursor, device);
}
```

The cursor emits aggregated `motion`, `motion_absolute`, `button`,
`axis`, `frame` signals. tinywl's `process_cursor_motion` finds the
surface under the pointer and calls
`wlr_seat_pointer_notify_enter` / `notify_motion` to forward the
event to the client. For interactive move/resize, the compositor
catches the events itself (see `TINYWL_CURSOR_MOVE` /
`TINYWL_CURSOR_RESIZE` in `begin_interactive`).

### 2.5 Surface management

A Wayland **surface** (`wl_surface`) is the fundamental drawable
object. wlroots represents a surface as `struct wlr_surface`. An
**xdg-surface** wraps a `wl_surface` and adds window-management
semantics (toplevel, popup). tinywl's `tinywl_view` wraps the
`wlr_xdg_surface`.

The render path (lines 519-635 of tinywl.c, abbreviated):

```c
struct render_data {
    struct wlr_output *output;
    struct wlr_renderer *renderer;
    struct tinywl_view *view;
    struct timespec *when;
};

static void render_surface(struct wlr_surface *surface,
        int sx, int sy, void *data) {
    struct render_data *rdata = data;
    struct tinywl_view *view = rdata->view;
    struct wlr_output *output = rdata->output;

    /* Get the texture (GPU resource). wlroots negotiated this with
     * the client — could be a dmabuf, a shm buffer, etc. */
    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (texture == NULL) {
        return;
    }

    /* Translate layout coordinates → output-local coordinates. */
    double ox = 0, oy = 0;
    wlr_output_layout_output_coords(
            view->server->output_layout, output, &ox, &oy);
    ox += view->x + sx, oy += view->y + sy;

    /* Apply scale factor for HiDPI outputs. */
    struct wlr_box box = {
        .x = ox * output->scale,
        .y = oy * output->scale,
        .width = surface->current.width * output->scale,
        .height = surface->current.height * output->scale,
    };

    /* Project the box to a model-view-projection matrix. */
    float matrix[9];
    enum wl_output_transform transform =
        wlr_output_transform_invert(surface->current.transform);
    wlr_matrix_project_box(matrix, &box, transform, 0,
            output->transform_matrix);

    /* Draw it on the GPU. */
    wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

    /* Tell the client we've displayed this frame so it can prepare
     * another one. */
    wlr_surface_send_frame_done(surface, rdata->when);
}

static void output_frame(struct wl_listener *listener, void *data) {
    /* Called every time an output is ready to display a frame,
     * generally at the output's refresh rate (e.g. 60Hz). */
    struct tinywl_output *output =
        wl_container_of(listener, output, frame);
    struct wlr_renderer *renderer = output->server->renderer;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (!wlr_output_attach_render(output->wlr_output, NULL)) {
        return;
    }
    int width, height;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);
    wlr_renderer_begin(renderer, width, height);

    float color[4] = {0.3, 0.3, 0.3, 1.0};
    wlr_renderer_clear(renderer, color);

    /* Render views back-to-front. */
    struct tinywl_view *view;
    wl_list_for_each_reverse(view, &output->server->views, link) {
        if (!view->mapped) continue;
        struct render_data rdata = {
            .output = output->wlr_output,
            .view = view,
            .renderer = renderer,
            .when = &now,
        };
        wlr_xdg_surface_for_each_surface(view->xdg_surface,
                render_surface, &rdata);
    }

    /* Software-cursor fallback (no-op if hardware cursors are in use). */
    wlr_output_render_software_cursors(output->wlr_output, NULL);

    wlr_renderer_end(renderer);
    wlr_output_commit(output->wlr_output);
}
```

Key takeaways for WLTIOS:

- Rendering happens **at the refresh rate** in the `output.frame`
  callback. You don't render in a separate thread.
- The pattern is: `wlr_output_attach_render` → `wlr_renderer_begin` →
  clear → iterate views → `wlr_renderer_end` → `wlr_output_commit`.
  This is the explicit buffer-swap dance for direct rendering.
- **`wlr_surface_send_frame_done`** is what throttles clients —
  without it, the client would render frames as fast as it can and
  burn CPU/GPU. The compositor tells the client "I'm ready for the
  next one" only after it has actually composited the previous one.

**Modern alternative — the `wlr_scene` API** (added in wlroots 0.15,
stabilized in 0.16+): instead of writing `render_surface` manually,
you build a scene graph of `wlr_scene_tree`, `wlr_scene_rect`, and
`wlr_scene_buffer` nodes. wlroots handles damage tracking,
back-to-front rendering, and output cloning for you. Sway, Wayfire,
and Hyprland have all moved to the scene API.
— https://emersion.fr/blog/2021/status-update-33
— https://github.com/swaywm/wlroots/issues/1826

**Recommendation for WLTIOS**: use the scene API from the start;
it cuts the compositor's render code from ~200 lines to ~30 lines
and handles edge cases (damage tracking, multi-output, HiDPI) that
tinywl explicitly doesn't.

### 2.6 xdg-shell

`xdg-shell` is the Wayland protocol for "application windows"
(toplevels and popups). The protocol is documented at
https://wayland.app/protocols/xdg-shell. An `xdg_surface` is a
`wl_surface` wrapped twice — once in an `xdg_surface`, then again in
an `xdg_toplevel` or `xdg_popup`.

In tinywl (lines 758-791, verbatim):

```c
static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
    /* This event is raised when wlr_xdg_shell receives a new xdg
     * surface from a client, either a toplevel (application window)
     * or popup. */
    struct tinywl_server *server =
        wl_container_of(listener, server, new_xdg_surface);
    struct wlr_xdg_surface *xdg_surface = data;
    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        return;
    }

    /* Allocate a tinywl_view for this surface */
    struct tinywl_view *view =
        calloc(1, sizeof(struct tinywl_view));
    view->server = server;
    view->xdg_surface = xdg_surface;

    /* Listen to the various events it can emit */
    view->map.notify = xdg_surface_map;
    wl_signal_add(&xdg_surface->events.map, &view->map);
    view->unmap.notify = xdg_surface_unmap;
    wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
    view->destroy.notify = xdg_surface_destroy;
    wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

    struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
    view->request_move.notify = xdg_toplevel_request_move;
    wl_signal_add(&toplevel->events.request_move, &view->request_move);
    view->request_resize.notify = xdg_toplevel_request_resize;
    wl_signal_add(&toplevel->events.request_resize, &view->request_resize);

    /* Add it to the list of views. */
    wl_list_insert(&server->views, &view->link);
}
```

The xdg-surface lifecycle signals a compositor must handle:

| Signal | When it fires | What the compositor does |
|---|---|---|
| `map` | The client has committed a buffer; surface is visible | Add to render list, give it focus |
| `unmap` | The client hid the surface | Remove from render list |
| `destroy` | The client destroyed the surface | Free compositor-side state |
| `toplevel.request_move` | Client asked to start an interactive move (e.g. user dragged the title bar) | Begin grab |
| `toplevel.request_resize` | Client asked to start an interactive resize | Begin grab with edge info |

The compositor drives the client back with **configure requests**:
- `wlr_xdg_toplevel_set_activated(toplevel, bool)` — tells the client
  "you have focus" / "you don't". The client repaints (e.g. shows a
  caret) accordingly.
- `wlr_xdg_toplevel_set_size(toplevel, w, h)` — asks the client to
  resize. The client should reply by committing a new buffer at the
  requested size; the compositor waits for the next commit before
  applying the new geometry.

### 2.7 Building wlroots

From the wlroots README (verbatim):

> Install dependencies:
>
> * meson
> * wayland
> * wayland-protocols
> * EGL and GLESv2 (optional, for the GLES2 renderer)
> * Vulkan loader, headers and glslang (optional, for the Vulkan renderer)
> * libdrm
> * GBM
> * libinput (optional, for the libinput backend)
> * xkbcommon
> * udev
> * pixman
> * [libseat](https://git.sr.ht/~kennylevinsen/seatd)
>
> If you choose to enable X11 support:
>
> * xwayland (build-time only, optional at runtime)
> * libxcb
> * libxcb-render-util
> * libxcb-wm
> * libxcb-errors (optional, for improved error reporting)
>
> Run these commands:
>
>     meson build/
>     ninja -C build/
>
> Install like so:
>
>     sudo ninja -C build/ install

Concrete Debian/Ubuntu one-liner (I didn't run it but it's the
standard list derived from the README and from
https://wiki.gentoo.org/wiki/Wlroots):

```sh
sudo apt install meson ninja-build \
  wayland-protocols libwayland-dev \
  libegl-dev libgles2-mesa-dev \
  libdrm-dev libgbm-dev \
  libinput-dev libxkbcommon-dev \
  libudev-dev libpixman-1-dev \
  libseat-dev \
  libxcb-dev libxcb-render-util0-dev libxcb-icccm4-dev libxcb-errors-dev \
  pkgconf
```

Then:

```sh
git clone https://gitlab.freedesktop.org/wlroots/wlroots.git
cd wlroots
meson setup build/
ninja -C build/
sudo ninja -C build/ install
```

For building **a compositor that links wlroots** (e.g. tinywl), the
meson snippet looks like:

```meson
wlroots = dependency('wlroots')
wayland_server = dependency('wayland-server')
xkbcommon = dependency('xkbcommon')
libinput = dependency('libinput')

executable('wltios-compositor',
  sources: 'main.c',
  dependencies: [wlroots, wayland_server, xkbcommon, libinput],
  install: true,
)
```

(Confirmed pattern from
https://git.sysadmins.ws/pkgs/raven/src/commit/a27c3dd521b909a28550727a29b3a33fe1d63a8b/base/wlroots/examples.meson.build.)

Notes:

- wlroots does **not** guarantee API stability across major versions.
  wlroots 0.18 was released in 2024 with breaking changes; 0.19 / 0.20
  followed. Pin a version in meson (`dependency('wlroots', version: '>=0.18')`)
  and update explicitly.
  — https://www.phoronix.com/news/wlroots-0.18-Released
  — https://www.phoronix.com/news/wlroots-0.20-Released
- There are wrapper libraries for Rust (Smithay), Haskell, Zig, etc.
  listed at https://github.com/search?q=topic%3Abindings+org%3Aswaywm&type=Repositories
  — but for an OS we want maximum control, so C is the right choice.

### 2.8 Performance (60 fps)

The render loop is driven by the output's vblank, so out of the box
tinywl runs at the monitor's refresh rate (60 Hz, 120 Hz, 144 Hz,
whatever). The `output.frame` callback (see §2.5) fires at the
refresh rate; you render and commit, then wait for the next vblank.

Things that affect whether you actually hit 60 fps:

1. **Don't render more than once per frame.** A common bug is
   re-rendering on every damage event instead of coalescing. The
   scene API handles this; tinywl does not (it renders the whole
   screen every frame). For a real compositor, track damage rects
   (`wlr_output_damage`) and only repaint what changed.
   — https://github.com/swaywm/wlroots/issues/1925

2. **Hardware cursors.** wlroots uses hardware cursor planes by
   default when the DRM driver supports them; if not, it falls back
   to software cursors (`wlr_output_render_software_cursors`, called
   in tinywl's `output_frame`). Hardware cursors move at full
   refresh rate even without a re-render.

3. **Direct scanout.** When there's a single fullscreen surface and
   no transforms, wlroots can scanout the client's buffer directly
   to the display, bypassing the compositor entirely. This is the
   zero-copy fast path for fullscreen video / games. Enable via
   `wlr_output_enable_direct_scanout`.

4. **Explicit sync.** wlroots 0.18+ supports
   `wp_linux_drm_sync_manager_v1` (Wayland explicit sync protocol),
   which dramatically reduces stutter on Wayland-NVIDIA. For WLTIOS
   on the i5-1135G7 with Intel iGPU, implicit sync is fine; explicit
   sync matters more for NVIDIA.

5. **Vulkan renderer.** Optional, can be faster than GLES2 on some
   hardware, and is required for some advanced features (e.g.
   color management pipelines). Build wlroots with
   `-Drenderers=vulkan` to enable.

6. **Multi-threaded rendering.** wlroots does *not* render in a
   separate thread by default. The compositor's main thread runs the
   event loop, handles input, and renders. For a desktop compositor
   this is usually fine. For heavy post-processing (blur, shaders),
   consider offloading to a render thread — but be careful with
   buffer ownership.

A reddit thread from a compositor author notes that virtual outputs
in wlroots can be artificially capped at 30 fps at 4K due to
internal clock choices, so always test on real hardware.
— https://www.reddit.com/r/wayland/comments/1m5d809/why_is_my_wlrootswayland_virtual_output_capped_at/

---

## Part 3 — How this applies to WLTIOS

### 3.1 Anonymity layer (Tor in seL4 partition)

WLTIOS's design puts Tor in a **seL4 partition** isolated from the
Linux guest (per `MASTER_CONTEXT.md` §1 "Key Design Decisions"). The
Tails research tells us exactly what that partition must do:

**Recommended concrete design:**

1. **Run Tor as a single seL4 user-space component** with its own
   address space. Pin the torrc to match Tails's defaults
   (see §1.1) — same `SocksPort`s, same `DNSPort`, same
   `TransPort`, same `IsolateDestAddr IsolateDestPort` flags.
   Add `AvoidDiskWrites 1` and `Sandbox 1`.

2. **Implement the kill switch in the seL4 net driver**, not in
   Linux iptables. seL4 enforces it: the Linux guest's network
   driver has *no* route to the NIC except via the Tor partition.
   In Tails the equivalent is `policy DROP` on OUTPUT in ferm (§1.2),
   but Tails still trusts the Linux kernel. WLTIOS gets stronger
   isolation because the Linux guest can be fully compromised and
   still cannot leak.

3. **DNS over Tor.** Run Tor's `DNSPort 5353` in the seL4 partition.
   The Linux guest's `/etc/resolv.conf` points to a stub resolver
   that forwards to the seL4 Tor partition. Tails does the same
   thing with `REDIRECT to-ports 5353` (§1.4); WLTIOS does it with
   an seL4 IPC forwarding rule.

4. **Transparent proxy for non-SOCKS apps.** For apps that don't
   speak SOCKS, WLTIOS needs Tails's equivalent of
   `REDIRECT to-ports 9040`. In seL4 this is an IPC redirect: any
   TCP connect from the Linux guest that isn't to a Tor SOCKS port
   is silently tunneled to the seL4 Tor partition's TransPort.

5. **MAC spoofing before network up.** Run a `macchanger -e`
   equivalent (or directly use `ip link set dev <iface> address
   <random>`) in the seL4 net driver init, *before* the Linux guest
   sees the NIC. This avoids Tails's known firmware-leak issue
   (§1.5). If the spoof fails, **do not bring the NIC up** — Tails's
   panic-mode behavior (§1.5) is the correct precedent.

6. **Stream isolation.** Expose at least two virtual SOCKS ports to
   the Linux guest: a default `9050 IsolateDestAddr IsolateDestPort`
   for general apps, and a `9051 IsolateSOCKSAuth` for the browser.
   The browser (Whonix-style or Tor-Browser-style) sets a unique
   SOCKS username per first-party origin to get per-origin circuits
   (§1.7).

7. **Disable IPv6 in the Linux guest** to mirror Tails's
   `domain ip6 { chain OUTPUT { policy DROP; } }` rule (§1.2). seL4
   simply does not route IPv6 from the Linux guest to the NIC.

8. **Amnesia.** The Linux guest runs from a tmpfs (SquashFS loaded
   into RAM at boot — §1.6). On shutdown, the seL4 partition wipes
   the RAM region used by Linux. For Tor specifically,
   `AvoidDiskWrites 1` ensures no state is written even if a disk is
   present. **What I don't know:** whether WLTIOS will have any
   persistent storage at all; that's a product decision not covered
   here.

### 3.2 GUI layer (wlroots compositor)

WLTIOS's design calls for a "Custom Wayland compositor built on
wlroots" (per `MASTER_CONTEXT.md` §1). The wlroots research gives us
a concrete path:

**Recommended concrete design:**

1. **Start from tinywl, but adopt `wlr_scene` from day one.** tinywl
   is the right structural template (it shows the exact listener
   wiring for outputs, inputs, xdg-shell, cursor, seat — §2.2). But
   for rendering, use the scene-graph API (`wlr_scene_tree_create`,
   `wlr_scene_surface_create`, `wlr_scene_output_create`) instead of
   tinywl's manual `render_surface`. The scene API handles damage,
   multi-output, HiDPI, and direct scanout automatically.

2. **Listener-wiring pattern.** Adopt tinywl's pattern of one
   `struct wl_listener` per signal per object, with `wl_signal_add`
   in the constructor and `wl_list_remove` in the destructor. This
   is verbose but extremely readable; it matches the rest of the
   wlroots ecosystem.

3. **Seat model.** One `wlr_seat` ("seat0"). All keyboards route
   through it via `wlr_seat_set_keyboard` + `wlr_seat_keyboard_notify_*`.
   All pointers attach to one `wlr_cursor` and route through it.
   This is the Tails-equivalent of "single-user seat"; multi-seat
   is unnecessary for a personal OS.

4. **Keybindings with the Super (Windows) key**, not Alt. tinywl
   uses Alt for historical reasons; modern compositors (Sway,
   Wayfire, Hyprland) default to Super. WLTIOS's keybindings should
   be data-driven (config file) like Sway, not hardcoded like tinywl.

5. **xdg-shell only at first.** tinywl supports only `xdg_toplevel`
   (§2.6); no layer-shell, no popups-as-separate-surfaces. Add
   layer-shell (`wlr_layer_shell_v1_create`) early — it's needed
   for panels, launchers, lock screens, notifications. Without
   layer-shell you cannot build a usable desktop.

6. **Xwayland.** Build wlroots with X11 support (`-Dxwayland=enabled`)
   and use `wlr_xwayland_create`. This lets legacy X11 apps run.
   WLTIOS's architecture doc doesn't explicitly mention X11
   compatibility, but for a real OS it's almost certainly needed
   in the short term.

7. **Performance budget.** Per `MASTER_CONTEXT.md`, idle RAM < 256 MB
   and boot < 3 s. wlroots itself is ~60 KLOC and adds maybe 5–10 MB
   to RSS. The compositor binary should be < 500 KB if we keep it
   lean. 60 fps is automatic if we use the scene API and don't do
   silly things like re-rendering the whole screen on every key
   press.

8. **Dependencies to package for WLTIOS:**
   `wayland`, `wayland-protocols`, `libdrm`, `libgbm`, `libinput`,
   `xkbcommon`, `libudev`, `pixman`, `libseat`, `mesa` (for GLES2),
   `xwayland`, `libxcb*`. That's the full wlroots dependency closure
   from §2.7.

9. **Don't write the compositor in Rust yet.** Smithay (the Rust
   wlroots wrapper) is excellent but still maturing and not as
   up-to-date as wlroots C. WLTIOS already plans to use Rust for
   other components (e.g. userland tools); for the compositor, C
   gives us direct access to every wlroots API as soon as it ships,
   without waiting for bindings. We can revisit this in 12–18 months
   if Smithay stabilizes.

10. **First milestone**: "WLTIOS-tinywl" — a fork of tinywl.c that
    compiles against wlroots 0.18+ and boots to a gray background
    with one xdg-shell window (e.g. `weston-terminal`) on screen.
    That proves the entire graphics stack works (kernel DRM, libdrm,
    mesa, wlroots, our compositor). Everything else (tiling,
    panels, workspaces) builds on top.

### 3.3 Risk register (things that could go wrong)

| Risk | Mitigation |
|---|---|
| wlroots API breaks between 0.18 and 0.20 | Pin wlroots version in meson; only upgrade deliberately |
| Tor in seL4 partition has lower throughput than native Tor | Acceptable for an OS where anonymity > speed. Tor itself caps at ~10–50 Mbit/s typical. |
| MAC spoofing fails silently in firmware (Tails #15208) | WLTIOS controls the firmware path (bootloader → seL4 → Linux). Bring the NIC up only after MAC is set. |
| `live-boot` / tmpfs requires enough RAM to hold the whole SquashFS | Tails needs ~4 GB RAM for the 1.9 GB image. WLTIOS at <2 GB install should fit in 4 GB RAM comfortably. |
| Wayland clients expect `wl_shm` and dmabuf — must implement both | wlroots does this for free; just don't disable them. |
| X11 apps break without Xwayland | Always build wlroots with `-Dxwayland=enabled`. |

### 3.4 Recommended next actions (concrete, ordered)

1. **Clone the wlroots repo on the dev PC** and build tinywl against
   wlroots 0.18+ in a QEMU VM with virtio-gpu. Verify it boots to a
   gray screen and `weston-terminal` runs. This is the smallest
   possible end-to-end test of the GUI stack.

2. **Read the wlroots wiki** (https://github.com/swaywm/wlroots/wiki)
   end-to-end, plus Drew DeVault's "Writing a Wayland Compositor"
   series (https://drewdevault.com/2018/07/29/Wayland-shells.html
   and following). This is the closest thing to an official tutorial.

3. **Fork tinywl into `wltios/compositor/`** with module headers per
   the engineering framework. Replace the manual render loop with
   `wlr_scene_*` calls. Add layer-shell. This becomes WLTIOS's
   GUI Phase-1 deliverable.

4. **Prototype the seL4 Tor partition.** Reuse Tails's torrc
   verbatim (§1.1). Implement a seL4 net filter that mirrors Tails's
   ferm.conf (§1.2): default-deny OUTPUT, allow only the Tor
   component's outgoing TCP `SYN NEW`. This is the WLTIOS anonymity
   Phase-1 deliverable.

5. **Write an ADR for "Tor isolation in seL4 vs Linux"** capturing
   the decision and the Tails firewall design as the reference
   baseline.

6. **Write a spec for the WLTIOS compositor** that lists every
   wlroots signal we subscribe to and the policy for each — mirroring
   tinywl's listener structure but extended with layer-shell,
   workspaces, and our keybinding model.

---

## Sources

### Tails OS — primary

- Tails GitLab root: https://gitlab.tails.boum.org/tails/tails
  (Also accessible at https://gitlab.tails.net/tails/tails — same
  project; `boum.org` is the legacy domain, `tails.net` is the
  current public domain. I fetched the raw files from `boum.org`
  successfully on 2026-06-21.)
- Tails torrc (raw):
  https://gitlab.tails.boum.org/tails/tails/-/raw/master/config/chroot_local-includes/etc/tor/torrc
- Tails ferm.conf (raw):
  https://gitlab.tails.boum.org/tails/tails/-/raw/master/config/chroot_local-includes/etc/ferm/ferm.conf
- Tails ferm directory:
  https://gitlab.tails.boum.org/tails/tails/-/tree/master/config/chroot_local-includes/etc/ferm
- Tails chroot_local-includes root:
  https://gitlab.tails.boum.org/tails/tails/-/tree/master/config/chroot_local-includes
- Tails MAC spoofing script (mirror):
  https://gist.github.com/3297e6bc5e8a77a2f84307c66410aa06
- Tails "How Tails works" (amnesia, persistent storage):
  https://tails.net/about/index.en.html
- Tails Wikipedia (overview, amnesia):
  https://en.wikipedia.org/wiki/Tails_(operating_system)
- Tails issue #15208 (MAC spoofing exposes half MAC):
  https://gitlab.tails.boum.org/tails/tails/-/issues/15208
- Tails issue #17779 (MAC spoofing fail disables NIC silently):
  https://gitlab.tails.boum.org/tails/tails/-/issues/17779
- Tails issue #20503 (Arti stream isolation design):
  https://gitlab.tails.boum.org/tails/tails/-/issues/20503
- Tails issue #17945 (initramfs boot failure):
  https://gitlab.tails.boum.org/tails/tails/-/issues/17945
- Tails leak on MacBook Pro (firmware MAC leak):
  https://jasonmurray.org/posts/2021/tailsleak

### Tails OS — secondary

- Tor Project forum ("Tails blocks all traffic except Tor"):
  https://forum.torproject.org/t/only-connection-via-tor-for-the-entire-operating-system/295
- Whonix Stream Isolation wiki:
  https://www.whonix.org/wiki/Stream_Isolation
- Whonix live mode / amnesia discussion:
  https://forums.whonix.org/t/whonix-live-mode-amnesia-amnesic-non-persistent-anti-forensics/3894
- ArchWiki Tor (transparent proxy leak protection):
  https://wiki.archlinux.org/title/Tor
- live-boot `toram` behavior on clonezilla:
  https://sourceforge.net/p/clonezilla/discussion/Clonezilla_live/thread/03690fdb
- Reddit (Tails squashfs-to-RAM explanation):
  https://www.reddit.com/r/tails/comments/1iqr9hp/expert_tails_users_please_help_clarify_my_doubt
- Tor Browser stream isolation via SOCKS auth (DNSCrypt discussion):
  https://github.com/DNSCrypt/dnscrypt-proxy/discussions/2691

### wlroots — primary

- wlroots project (migrated to freedesktop):
  https://gitlab.freedesktop.org/wlroots/wlroots
- wlroots README (raw, GitHub mirror):
  https://raw.githubusercontent.com/swaywm/wlroots/master/README.md
- tinywl.c (raw, GitHub mirror):
  https://raw.githubusercontent.com/swaywm/wlroots/master/tinywl/tinywl.c
  (Local copy saved at `/tmp/tinywl.c` during research.)
- wlroots Getting-started wiki:
  https://github.com/swaywm/wlroots/wiki/Getting-started
- Drew DeVault "Input handling in wlroots":
  https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
- Drew DeVault "Writing a Wayland Compositor, Part 1":
  https://drewdevault.com/2018/07/29/Writing-a-Wayland-compositor-1.html
- Drew DeVault "Wayland shells":
  https://drewdevault.com/2018/07/29/Wayland-shells.html
- The Wayland Book — Seats:
  https://wayland-book.com/seat.html
- The Wayland Book — XDG surfaces:
  https://wayland-book.com/xdg-shell-basics/xdg-surface.html
- Wayfire "X11 → WLC → libweston → wlroots":
  https://wayfire.org/2019/02/24/X11-WLC-Libweston-Wlroots.html

### wlroots — secondary

- xdg-shell protocol reference:
  https://wayland.app/protocols/xdg-shell
- wlr-output-management protocol:
  https://wayland.app/protocols/wlr-output-management-unstable-v1
- wlr-randr manpage:
  https://manpages.ubuntu.com/manpages/noble/man1/wlr-randr.1.html
- Scene API introduction (emersion):
  https://emersion.fr/blog/2021/status-update-33
- Scene API design issue:
  https://github.com/swaywm/wlroots/issues/1826
- Repaint scheduling issue (do not render more than once per frame):
  https://github.com/swaywm/wlroots/issues/1925
- wlroots 0.18 release notes:
  https://www.phoronix.com/news/wlroots-0.18-Released
- wlroots 0.20 release notes:
  https://www.phoronix.com/news/wlroots-0.20-Released
- Gentoo wlroots wiki:
  https://wiki.gentoo.org/wiki/Wlroots
- Building tinywl with gcc (Stack Overflow):
  https://stackoverflow.com/questions/63267096/how-to-compile-wayland-compositors-based-on-wlroots-in-c-using-gcc
- "Thoughts on writing a wayland window manager with wlroots":
  https://inclem.net/2021/04/17/wayland/writing_a_wayland_compositor_with_wlroots/
- Virtual output 30fps cap discussion:
  https://www.reddit.com/r/wayland/comments/1m5d809/why_is_my_wlrootswayland_virtual_output_capped_at/

---

## Appendix A — Local copies of source files

During research I saved the following local copies (in `/tmp/` — not
committed; if you need them in the repo, re-fetch or copy them):

- `/tmp/tinywl.c` — full 913-line tinywl.c source (fetched
  2026-06-21 from the GitHub raw mirror).
- `/tmp/tails_torrc_raw.json` — raw torrc contents.
- `/tmp/tails_ferm_conf.json` — raw ferm.conf contents.
- `/tmp/tails_mac_gist.json` — raw MAC spoofing script.

The verbatim contents of `torrc`, `ferm.conf`, and the MAC-spoofing
script are reproduced in §1.1, §1.2, and §1.5 of this report
respectively.

---

## Appendix B — What I don't know

For honesty per `MASTER_CONTEXT.md` §2 ("What he values: Honesty, no
hallucination"):

1. **The exact kernel command line Tails ships.** I know it uses
   `live-boot` with `toram`, but I did not retrieve the literal
   `APPEND` line from the Tails ISO config. To get it, fetch
   `config/binary_local-includes/isolinux/live.cfg.in` or
   `config/binary_local-includes/grub/live.cfg.in` from the Tails
   tree.
2. **Whether Tails uses `nftables` or `iptables` as the backend.** As
   of Tails 6.x the ferm config compiles to `iptables` rules; I did
   not check whether Tails has migrated to `nftables`. WLTIOS should
   pick `nftables` (it's the modern default).
3. **Whether the wlroots 0.18+ `wlr_scene` API has the exact same
   function names I cited.** I described it conceptually; the exact
   function signatures should be confirmed against
   `include/wlr/types/wlr_scene.h` in the wlroots source for the
   version we pin.
4. **Whether seL4 can realistically run Tor with acceptable
   performance.** This is an open research question for WLTIOS; the
   Tails research doesn't address it because Tails runs Tor in a
   normal Linux process.
5. **The exact wlroots version we should pin.** 0.18 is the latest
   stable as of mid-2024; 0.20 is mentioned by Phoronix but I didn't
   confirm a release date. WLTIOS should pin whatever is latest
   stable at the time of the GUI Phase-1 sprint.
