# WLTIOS — Glossary, Module Reference, Build/User Notes (Consolidated Reference)

> **Supersedes:** `docs/MODULE_MAP.md`, `docs/FILETREE.md`,
> `docs/dev-guide/build-from-source.md`, `docs/user-guide/getting-started.md`,
> `docs/community/website-content.md`,
> `src/userspace/wlt-defender/MANIFEST.md`. Archived under
> `_ARCHIVED_2026-07-29`. For actual module completeness status, see
> `docs/STATUS_ROADMAP.md` - this document is deliberately just names,
> locations, and build mechanics, not a second status tracker.

---

## 1. Glossary

| Term | Meaning |
|---|---|
| **seL4** | Formally verified microkernel; the intended (not yet fully realized) security foundation - see `docs/ARCHITECTURE.md` |
| **PD (Protection Domain)** | An seL4-isolated component (Key Vault PD, TorPD, Boot Verifier PD, Firewall PD) |
| **Track 1/2/3** | This consolidation's naming for the project's 3 parallel, unreconciled build configurations (seL4 multi-PD 32-bit / desktop-no-seL4 64-bit / bare 64-bit seL4) - see ADR-10 |
| **Kairos** | The OS's AI assistant (chat + tool use), distinct from Guardian |
| **Guardian / wlt-defender** | The always-on, non-LLM endpoint security engine |
| **Amnesic mode** | tmpfs-backed `/tmp`, `/var`, `/run`, `/home` - wiped on reboot. Passive only; no active secure-wipe of in-kernel key material yet |
| **Kill switch** | Route-deletion (`SIOCDELRT`) fallback that blocks all non-Tor egress if Tor disconnects |
| **A/B update** | Two-slot (`wlt-root-a`/`b`) atomic update scheme with anti-rollback counter, signed manifest, and (foundation-only) dm-verity |
| **Ultrareview** | This project's own internal name for its adversarial security-review passes (cited in code comments, e.g. `launch.c`'s fail-closed fix) |

## 2. Module map (name, location, language - status lives in `STATUS_ROADMAP.md`)

| Module | Path | Language (actual, not spec'd) |
|---|---|---|
| WLT-Boot | `src/boot/wlt-boot/` | C + inline asm |
| WLT-Init | `src/userspace/wlt-init/` | C |
| WLT-Compositor | `src/userspace/wlt-compositor/` | C (wlroots 0.18) |
| WLT-Shell | `src/userspace/wlt-shell/` | C |
| WLT-Terminal | `src/userspace/wlt-terminal/` | C |
| WLT-FileManager | `src/userspace/wlt-filemanager/` | C |
| WLT-Settings | `src/userspace/wlt-settings/` | C |
| WLT-Net | `src/userspace/wlt-net/` | C (spec said Rust) |
| WLT-Sandbox | `src/userspace/wlt-sandbox/` | C (spec said Rust) |
| WLT-Permissions | `src/userspace/wlt-permissions/` | C |
| WLT-Pkg | `src/userspace/wlt-pkg/` | C (spec said Rust) |
| WLT-Update | `src/userspace/wlt-update/` | C (spec said Rust) |
| WLT-Crypto | `src/userspace/wlt-crypto/` | C |
| WLT-Tor-Mgr | `src/userspace/wlt-tor-mgr/` | C |
| WLT-AI (Kairos) | `src/userspace/wlt-ai/` | C |
| WLT-Defender (Guardian) | `src/userspace/wlt-defender/` | C + eBPF |
| Key Vault PD, TorPD, Boot Verifier PD, Firewall PD | `src/sel4-partitions/` (naming per spec; verify actual path before editing) | C (bare-metal) |

## 3. Building from source

Core toolchain (per the archived dev-guide, still the right starting point):
WSL2 Ubuntu 24.04+, GCC 13+, CMake 4.0+, Ninja, Python 3.12+, QEMU 10.0+,
wlroots built from source (`0.18.2` tag) with its own dependency list
(wayland-protocols, libxkbcommon, Mesa, libdrm, gbm, pixman, libseat,
libinput, etc.) The per-module test-compile recipe (looping over modules,
compiling each `_test.c` against its module's real `.c` files with
`-Wall -Wextra -Werror`) is a real, useful pattern worth keeping as-is.

**wlt-defender / Guardian has its own, separate toolchain** (documented in
its own `DEV_ENV.md`, folded into `docs/AI_KAIROS.md` §3.2): micromamba/
conda-forge for clang/llvm/lld (BPF compilation needs `-target bpf`),
libbpf and YARA built from source, kernel UAPI headers via
`apt-get download linux-libc-dev` (no root needed). If working on Guardian
specifically, read that section before assuming the main build guide covers
it - it doesn't.

**wlt-integration** (`src/userspace/wlt-defender/wlt-integration/README.md`,
not read in full during this pass) is the handoff doc for bundling Guardian
into the main initramfs build - check it directly before attempting that
integration rather than relying on this summary.

## 4. Known adding-a-module workflow (still applicable)

1. Write a spec in `docs/specs/` (note: post-consolidation, per-module specs
   that were purely design/target content were archived where their
   substance is now folded into the relevant consolidated doc; if adding a
   genuinely new, not-yet-built module, a fresh spec file is still the
   right artifact - it isn't redundant with these 10 docs, which describe
   what exists and its verified status, not un-built target designs)
2. Create `src/userspace/wlt-yourmodule/` with the standard layout
   (`include/`, `.c` files, Makefile following the wlt-compositor pattern)
3. Write a test in `tests/userspace/yourmodule_test.c`
4. Add to the module list above and to the relevant per-topic consolidated
   doc (not a new status file - extend `STATUS_ROADMAP.md`)
5. Add to `config/services/wlt-yourmodule.service`
6. Commit with the project's established message format:
   `[module] add: wlt-yourmodule — <description>`

## 5. User-facing feature summary (target design - cross-check against `STATUS_ROADMAP.md` before promising any of this works today)

Live USB or hard-drive install; Tor-routed networking by default with a
kill switch; amnesic mode; MAC randomization; per-app privacy badges;
Kairos AI assistant (hotword "Wake up, Buddy!"); Settings app covering
network/display/sound/security/privacy/accounts/updates/apps/system.
**Before quoting any of this to an end user, confirm current status in
`docs/STATUS_ROADMAP.md` and the relevant topic doc** - several of these
(Settings app completeness, hotword detection quality, full install-to-disk
flow) are explicitly flagged elsewhere in this doc set as unverified or
placeholder-quality.

## 6. Community / project info

Repository: `https://github.com/onlykushalll/WLTIOS`. Started June 2026.
Built by the project owner with AI assistance across multiple systems
(see `docs/PROCESS_METHODOLOGY.md` §3). License: GPL-2.0 for seL4-derived
components, project-specific for original userspace code (per the
top-level `README.md`, which was left in place, not archived - it's a
repo-front-page artifact, not a design/status doc, and was out of scope
for this consolidation's mandate).
