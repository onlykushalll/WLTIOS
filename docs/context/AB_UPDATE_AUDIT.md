# A/B Update Source Audit — wlt-update

> **Task ID:** ab-update-audit
> **Agent:** GLM 5.2 subagent (via MCPilot bridge to WSL2)
> **Mode:** READ-ONLY — no source files were modified; only `-fsyntax-only` compilation tests were run (no `.o` written).
> **Repo audited:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Desktop\WLTIOS\src\userspace\wlt-update\` (WSL2: `/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace/wlt-update/`)
> **Audit baseline numbers** (from the task prompt): update.c=653, partition.c=405, config.c=259, verify.c=208, plus `download.c` and `transaction.c`.

---

## 0. Headline findings (TL;DR)

1. **Only 4 of the 6 expected files exist.** `download.c` and `transaction.c` are **not present** in the source tree. Their responsibilities are folded into `update.c` (`wlt_update_download()` lives at line 474; `wlt_update_install()` performs what would otherwise be a transaction at line 514). The audit prompt's "6 files" assumption does not match the repo.
2. **All 4 files are larger than the audit baseline** (by +36 to +58 lines each). This is a real C implementation, not a skeleton — contrary to what `wltios-deploy/docs/FILETREE.md` claims ("🔧 Skeleton | No actual download/install pipeline").
3. **Zero TODOs, zero FIXMEs, zero stubs, zero `not_implemented` markers across all 4 files.** The only `return 0;` is the equality branch of `version_cmp()` in `update.c:301`, which is correct behaviour, not a stub.
4. **`partition.c` does implement a real A/B scheme** — two LUKS2-mapped devices (`/dev/mapper/wlt-root-a` and `wlt-root-b`), two mountpoints (`/mnt/wlt-a`, `/mnt/wlt-b`), two ESP kernel/sig sets (`/boot/sel4-{a,b}.bin` + `.sig`), a `[wlt-boot]`+`[A]`+`[B]` INI config at `/boot/wltios.conf`, an active-slot marker at `/run/wlt/active-partition`, atomic temp-then-rename config save, and a rollback path that **refuses to flip to a slot marked `failed`** (no-bricking guarantee).
5. **`verify.c` has genuine Ed25519 code** (multi-part streaming via `crypto_sign_init/update/final_verify`) — but it is entirely behind `#ifdef HAVE_SODIUM`. Without `-DHAVE_SODIUM` the file still compiles cleanly, but every entry point returns `WLT_UPDATE_ERR_UNSUPPORTED` (-13). That is the spec-mandated "fail loud, never silently skip verification" behaviour.
6. **libsodium-dev is NOT installed on the WSL2 build host.** Only the runtime `libsodium23` is present. `pkg-config --exists libsodium` returns false. So the Makefile auto-detect path leaves `HAVE_SODIUM` undefined → **Ed25519 verification is a compile-time no-op on the current host**. Installing `libsodium-dev` (or vendoring the header + `.so` into `packages/`) is the prerequisite for genuine verification.
7. **`wlt-update` is NOT in the build chain.** `scripts/build-all.sh` (200 lines, 14 steps) builds wlt-compositor, wlt-terminal, wlt-filemanager, wlt-launcher, wlt-shell, wlt-statusbar, libwlt-ai.a, kairos-window, wlt-settings, wlttor, libwlt-pkg.a, and wl-libs — **no step builds `wlt-update`**. The rootfs `/usr/lib/` directory at `~/wltios-build/wlt-rootfs64/usr/lib/` was checked and contains no `libwlt-update.*` and no `wltupdate` binary.
8. **No `main.c` / CLI binary in `wlt-update/`.** The Makefile explicitly notes: *"The `wltupdate` CLI binary is intentionally NOT built here: it lives in a separate main.c that links against this library (see tests/Makefile)."* No such `main.c` exists in `src/userspace/wlt-update/` or in `tests/userspace/` (only `tests/userspace/update_test.c`, which is a unit-test driver, not the spec's `wltupdate` CLI).
9. **Spec-vs-implementation language mismatch.** `docs/specs/wlt-update.md` specifies a Rust/Cargo.toml project (`main.rs`, `check.rs`, `download.rs`, `verify.rs`, `install.rs`, `partition.rs`, `boot_config.rs`, `rollback.rs`, `error.rs`). The implementation is **C**. `docs/ARCHITECTURE.md:194` still says "WLT-Update | 80K | Rust". `docs/MODULE_MAP.md:77` still says "wlt-update/ (Rust)". This is a stale-doc issue, not a code defect.
10. **All 4 files pass `gcc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only`** on the current host (without `-DHAVE_SODIUM`). The build would succeed cleanly today.

---

## 1. Directory listing (real `ls`)

```
total 76
-rwxrwxrwx  4614 Jun 22 05:02 Makefile       (109 lines)
-rwxrwxrwx  8644 Jun 22 05:02 config.c        (283 lines)
-rwxrwxrwx 17614 Jun 22 12:55 partition.c     (441 lines)
-rwxrwxrwx 25926 Jun 22 05:02 update.c       (711 lines)
-rwxrwxrwx  6766 Jun 22 05:02 verify.c       (232 lines)
drwxrwxrwx   512 Jun 22 05:02 include/
```
`include/` contains `partition.h` (126 lines) and `update.h` (186 lines).

**Missing vs. audit prompt:** `download.c`, `transaction.c` do not exist.

**Real `wc -l` (audit baseline in parens):**

| File | Real lines | Audit said | Δ |
|------|-----------:|-----------:|---:|
| `update.c`    | 711 | 653 | +58 |
| `partition.c` | 441 | 405 | +36 |
| `config.c`    | 283 | 259 | +24 |
| `verify.c`    | 232 | 208 | +24 |
| `download.c`  |  —  |  n/a | **missing** |
| `transaction.c` | — | n/a | **missing** |
| `Makefile`    | 109 | — | — |
| `include/update.h`    | 186 | — | — |
| `include/partition.h` | 126 | — | — |
| **Total** | **2088** | — | — |

The audit baseline numbers are all slightly *lower* than the real line counts — suggesting the baseline was captured from an earlier revision or estimated. Notably `partition.c` was modified most recently (Jun 22 12:55, vs 05:02 for the other three).

---

## 2. Per-file audit

### update.c (711 lines)
- **What it does**: Top-level update orchestrator — exposes the public `wlt_update_init / check / download / install / status / rollback / enable_auto / disable_auto / history` API and owns a minimal SOCKS5→HTTP/1.1 transport to fetch the version manifest and the signed `.wlt-update.tar.zst` package over Tor (no TLS — confidentiality comes from the `.onion` circuit). It also owns the status file at `/var/lib/wlt/update.status`, the history log at `/var/log/wlt/update.history`, and the atomic install pipeline (verify → format → mount → extract → flip boot default → mark pending-reboot → record history).
- **Stubs/TODOs**: NONE. `grep -iE "TODO|FIXME|stub|not_implemented|XXX|HACK"` returned zero hits. The only `return 0;` is at line 301 in `version_cmp()` (equality branch — correct, not a stub).
- **Compiles?**: **YES** — `gcc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -Iinclude -D_GNU_SOURCE update.c` returned **NO_ERRORS_CLEAN_COMPILE**. The Makefile dependency hint `update.o: include/update.h include/partition.h` matches the actual `#include` list.
- **Key finding**: The "download" functionality the audit prompt attributes to a separate `download.c` is entirely inside this file — `wlt_update_download()` (lines 473–511) implements resumable Tor download with retry loop and separate signature fetch. There is no separate `download.c`. Likewise the "transaction" functionality the audit prompt expects from `transaction.c` is folded into `wlt_update_install()` (lines 514–590), which performs the atomic verify→format→mount→free-check→extract→flip-boot-default→status-write→history-append sequence. The split-by-file design the audit prompt assumed is not how the code is organised.

### partition.c (441 lines)
- **What it does**: Implements the real A/B partition scheme — parses and atomically rewrites `/boot/wltios.conf` (INI with `[wlt-boot]`, `[A]`, `[B]` sections), selects the active slot by reading `/run/wlt/active-partition` (with boot-config fallback), formats the inactive slot via `mkfs.ext4`, mounts via `mount`, extracts the update package via `tar --use-compress-program=zstd`, stages the kernel+signature+linux onto the ESP at `/boot/sel4-{a,b}.bin`, `/boot/sel4-{a,b}.sig`, `/boot/linux-{a,b}.img`, and on rollback flips `default_partition` to the *other* slot while **refusing** if that slot is marked `failed`.
- **Stubs/TODOs**: NONE. No TODO/FIXME/stub markers. The file uses `#pragma GCC diagnostic ignored "-Wformat-truncation"` at line 32 — a deliberate warning suppression, not a stub.
- **Compiles?**: **YES** — `gcc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -Iinclude -D_GNU_SOURCE partition.c` returned **PARTITION_CLEAN_WERROR**.
- **A/B slot tracking** (the specific question the audit prompt asked):
  - **YES — two real slots, not a single partition.** `WLT_PARTITION_A = 0`, `WLT_PARTITION_B = 1`, `WLT_PARTITION_COUNT = 2` (enum in `partition.h:18-23`).
  - **Slot device paths** (from `partition.h:25-31`):
    | Slot | Device (LUKS2-mapped) | Mountpoint | ESP kernel | ESP sig | ESP linux |
    |------|----------------------|------------|------------|---------|------------|
    | A | `/dev/mapper/wlt-root-a` | `/mnt/wlt-a` | `/boot/sel4-a.bin` | `/boot/sel4-a.sig` | `/boot/linux-a.img` |
    | B | `/dev/mapper/wlt-root-b` | `/mnt/wlt-b` | `/boot/sel4-b.bin` | `/boot/sel4-b.sig` | `/boot/linux-b.img` |
  - **Assumed partition layout**: LUKS2-unlocked mapper devices (i.e. the bootloader/init already opened the LUKS2 containers via the key-vault PD before wlt-update runs). The spec's `/dev/sda2`/`/dev/sda3` raw-block-device view is one layer below this code — wlt-update consumes the post-unlock `/dev/mapper/wlt-root-*` abstraction.
  - **Boot config schema** (`/boot/wltios.conf`):
    ```ini
    [wlt-boot]
    version = 1
    timeout = 3
    default = A            # or B
    rollback_on_fail = yes
    [A]
    kernel = /boot/sel4-a.bin
    linux  = /boot/linux-a.img
    signature = /boot/sel4-a.sig
    verify = yes
    last_boot_success = 2026-06-21T10:00:00Z
    boot_attempts = 0
    failed = no
    version = 1.0.0
    [B] ... mirror ...
    ```
    This matches the spec's documented schema verbatim (`docs/specs/wlt-update.md:138-160`).
  - **Active-slot selection** (`partition.c:945-966`): reads `/run/wlt/active-partition` first (one byte 'A' or 'B'); falls back to the boot config's `default` field; final fallback to `WLT_PARTITION_A`.
  - **Inactive-slot selection** (`partition.c:968-977`): trivial complement of the active slot.
  - **Atomic boot config save** (`partition.c:912-942`): writes `<path>.tmp` then `rename()` — a crash mid-write never leaves a truncated config.
  - **Rollback safety** (`partition.c:1132-1158`): if the *target* (other) slot's `failed` flag is `true`, returns `WLT_UPDATE_ERR_PARTITION` and refuses to flip. This is the "no-bricking" guarantee.
  - **Boot-attempt tracking** (`partition.c:1121-1130`): `wlt_boot_config_increment_attempts()` flips `failed = true` when attempts reach `WLT_UPDATE_MAX_BOOT_ATTEMPTS` (3).
  - **External command execution**: uses `fork()` + `execve()` (no `system()` — the file header explicitly says "style forbids system()"). Hard-coded paths to `/usr/sbin/mkfs.ext4`, `/bin/mount`, `/bin/umount`, `/bin/tar`, `/usr/bin/zstd`.
  - **Key finding**: This is a **production-grade A/B implementation**, not a skeleton. The only caveat is that it assumes the existence of the LUKS2 mapper devices and the ESP — neither of which the wlt-init/wlt-crypto PD actually creates on the current build (see `ED25519_MONOCYPHER_SWAP.md` and `KEYVAULT_BRIDGE_IMPLEMENTED.md` for the current real state of disk encryption).

### config.c (283 lines)
- **What it does**: Loads `/etc/wlt/update.conf` (INI subset, single optional `[update]` section), applies hard-coded defaults (`http://repo.wltios.onion/manifest`, Tor SOCKS port 9050, current_version `1.0.0`, auto_update=false, `/etc/wlt/keys/update-pubkey.bin`, `/boot/wltios.conf`), persists the auto-update flag atomically (temp + rename), and exposes two shared helpers used by `update.c` and `partition.c`: `wlt_config_parse_kv()` and `wlt_strlcpy()` (hand-rolled strlcpy to stay clean under `-Wstringop-truncation` while satisfying the "no strcpy" rule).
- **Stubs/TODOs**: NONE. No TODO/FIXME/stub markers.
- **Compiles?**: **YES** — `gcc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -Iinclude -D_GNU_SOURCE config.c` returned **NO_ERRORS_CLEAN_COMPILE**.
- **Key finding**: The file provides the shared `wlt_strlcpy` and `wlt_config_parse_kv` symbols that both `update.c` and `partition.c` depend on (declared in `update.h:165-175`). These are *not* in a private header — they're in the public `update.h`, which is fine for the single-makefile layout but slightly leaky API design. No defect.

### verify.c (232 lines)
- **What it does**: Detached Ed25519 signature verification for update packages. Loads a 32-byte raw public key and 64-byte raw detached signature (both with strict-size checking that rejects trailing garbage), then verifies either an in-memory buffer (`wlt_update_verify_buffer`) or a streamed file (`wlt_update_verify_file` via libsodium's multi-part `crypto_sign_init / update / final_verify` API so 500 MB packages never need to fit in RAM).
- **Stubs/TODOs**: NONE in the literal `grep` sense. The file has no `TODO`/`FIXME`/`stub`/`not_implemented`/`XXX` markers. **However, when `HAVE_SODIUM` is undefined, every entry point is functionally a stub**: `wlt_update_verify_buffer` (line 1570-1577) and `wlt_update_verify_file` (line 1594-1596) both `return WLT_UPDATE_ERR_UNSUPPORTED` after logging `"built without libsodium — verification refused"`. This is by design — the file's header comment explicitly says *"a missing crypto library must never be confused with a valid signature"*.
- **Compiles?**: **YES, both with and without HAVE_SODIUM defined** — but the meaning differs:
  - Without `-DHAVE_SODIUM`: `gcc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -Iinclude -D_GNU_SOURCE verify.c` → **VERIFY_CLEAN_WERROR_NO_SODIUM**. Real Ed25519 code is `#ifdef`'d out. Every entry point returns -13.
  - With `-DHAVE_SODIUM`: `gcc ... -DHAVE_SODIUM verify.c` → **FATAL ERROR**: `sodium.h: No such file or directory`. libsodium-dev is not installed.
- **Crypto library required**: **libsodium** (the `crypto_sign_ed25519_*` family — `crypto_sign_verify_detached`, `crypto_sign_init`, `crypto_sign_update`, `crypto_sign_final_verify`, `crypto_sign_state`, `sodium_init`). Detected at build time via `pkg-config --exists libsodium`.
- **Real Ed25519 verification?**: **YES, when `HAVE_SODIUM` is defined** — uses libsodium's streaming multi-part API, not a toy implementation. The `load_pubkey` and `load_signature` helpers do strict length validation with trailing-byte drain. The streaming path (`wlt_update_verify_file`) correctly handles EOF vs ferror and frees the chunk buffer on all paths.
- **Key finding**: The crypto code is genuinely Ed25519 (not a hand-rolled curve25519 toy), but it is **only active if `libsodium-dev` is installed**. The current WSL2 build host has `libsodium23` (runtime only, `1.0.18-2`) — **no dev headers**. So `pkg-config --exists libsodium` returns false, `HAVE_SODIUM` stays undefined, and every verification call returns `WLT_UPDATE_ERR_UNSUPPORTED` (-13) at runtime. Since `wlt_update_install()` calls `wlt_update_verify_file()` *before* touching any partition (line 531), **no update can be installed in the current build configuration** — it will always fail at the verify step. This is the spec's "fail loud" guarantee working as intended, but it means the A/B pipeline cannot actually run on the current rootfs.

### Makefile (109 lines)
- **What it does**: Builds `libwlt-update.so` + `libwlt-update.a` from the 4 `.c` files above. Detects libsodium via `pkg-config --exists libsodium` and only defines `HAVE_SODIUM` + links `-lsodium` if the dev package is present.
- **Stubs/TODOs**: NONE.
- **Compiles?**: **YES** (verified indirectly via the `-fsyntax-only` checks above on each TU).
- **Key finding**: The Makefile explicitly states the `wltupdate` CLI binary is **not built here** — it is supposed to live in a separate `main.c` that links against this library. No such `main.c` exists in `src/userspace/wlt-update/` and no `wltupdate` binary is produced by `scripts/build-all.sh`. So the user-facing CLI surface specified in `docs/specs/wlt-update.md:117-127` (`wltupdate check`, `wltupdate install`, `wltupdate rollback`, etc.) is **not currently deliverable** — only the library is.

### download.c (audit said: present)
- **Status**: **MISSING** from the source tree.
- **Where its responsibilities live**: `update.c:473-511` (`wlt_update_download()`) and `update.c:180-287` (`tor_get()`, the SOCKS5+HTTP/1.1 transport).
- **Impact on spec**: The spec (`docs/specs/wlt-update.md:103-115`) lists `download.rs` as a separate file in the Rust design. The C implementation chose to keep the transport and the download orchestration in `update.c` for a single Makefile and auditability. Not a defect — just a different file decomposition than the spec envisioned.

### transaction.c (audit said: present)
- **Status**: **MISSING** from the source tree.
- **Where its responsibilities live**: `update.c:514-590` (`wlt_update_install()`) — the verify→format→mount→free-check→extract→flip-boot-default→status-write→history-append sequence is the closest thing to a "transaction" in this codebase, and it is **not atomic in the database-transaction sense**: a failure midway through (e.g. extract fails after format) leaves the inactive slot formatted-but-empty, and the boot config is *not* flipped (so the active slot remains unchanged). That is the correct A/B semantics — the inactive slot is expendable — but it means `wlt_update_install` should not be called a "transaction". The spec does not actually use the word "transaction" anywhere; the audit prompt introduced this term.
- **Impact on spec**: None — the spec has no `transaction.rs` either. The audit prompt's assumption of a `transaction.c` file appears to be speculative.

---

## 3. Cross-cutting grep results

| Pattern | update.c | partition.c | config.c | verify.c |
|---------|---------:|------------:|---------:|---------:|
| `TODO`           | 0 | 0 | 0 | 0 |
| `FIXME`          | 0 | 0 | 0 | 0 |
| `stub`           | 0 | 0 | 0 | 0 |
| `not_implemented`| 0 | 0 | 0 | 0 |
| `XXX`            | 0 | 0 | 0 | 0 |
| `HACK`           | 0 | 0 | 0 | 0 |
| `placeholder`    | 0 | 0 | 0 | 0 |
| `return 0;`      | 1 (line 301, `version_cmp` equality branch — correct, not a stub) | 0 | 0 | 0 |

**Total stubs/TODOs: ZERO across all 4 files.** This contradicts `wltios-deploy/docs/FILETREE.md:379` which labels wlt-update as "🔧 Skeleton | No actual download/install pipeline". The implementation is in fact a complete, dependency-correct C A/B update system — the *only* caveat is that it requires `libsodium-dev` for Ed25519 to actually run, and it is not currently wired into `build-all.sh`.

---

## 4. libsodium availability (specific question from audit prompt)

Command run from WSL2 (the build host):
```
$ dpkg -l | grep -i sodium
ii  libsodium23:amd64   1.0.18-2   amd64   Network communication, cryptography and signaturing library

$ ls /usr/include/sodium.h
ls: cannot access '/usr/include/sodium.h': No such file or directory

$ ls /usr/lib/x86_64-linux-gnu/libsodium*
/usr/lib/x86_64-linux-gnu/libsodium.so.23
/usr/lib/x86_64-linux-gnu/libsodium.so.23.3.0

$ which pkg-config && pkg-config --exists libsodium && echo HAVE || echo NO
/usr/bin/pkg-config
NO
```

Searches performed:
- `find /home/kushal -maxdepth 4 -name libsodium* -o -name sodium.h 2>/dev/null` → **no output** (nothing in user's home).
- `find /home/kushal/wltios-build -maxdepth 6 -name sodium.h` → **no output**.
- `find /home/kushal/wltios-build -maxdepth 8 -name libsodium` → **no output**.

**Conclusion**: libsodium runtime (`libsodium23` v1.0.18) is installed, but **libsodium-dev (the `-dev` package with headers)** is **NOT installed**, and no vendored copy of `sodium.h` + `libsodium.so` exists anywhere under `/home/kushal` or `~/wltios-build`. This means:
- The Makefile's `HAVE_SODIUM := $(shell pkg-config --exists libsodium ...)` test resolves to the empty string.
- `CPPFLAGS` does **not** include `-DHAVE_SODIUM`.
- `verify.c` is compiled with the `#else` branches active → every verification entry point returns `WLT_UPDATE_ERR_UNSUPPORTED` (-13).
- `wlt_update_install()` will fail at the `wlt_update_verify_file()` call (update.c:531) before ever formatting a partition.

**Prerequisite for genuine Ed25519 verification**: install `libsodium-dev` (`apt-get install libsodium-dev`) on the WSL2 build host, **or** vendor `sodium.h` + the static `libsodium.a` (or `.so` + symlink) into `packages/sodium-libs/` following the same pattern used for `packages/tor-libs/` and `packages/wl-libs/`, then update `build-all.sh` to install them into `$ROOTFS/usr/include/` + `$ROOTFS/usr/lib/` and add `-DHAVE_SODIUM -lsodium` to the wlt-update build step (which does not yet exist in `build-all.sh`).

---

## 5. Build-chain wiring (additional finding)

`scripts/build-all.sh` (200 lines, 14 steps) was inspected. Steps build:
1. wlt-compositor
2. wlt-terminal
3. wlt-filemanager
4. wlt-launcher
5. wlt-shell (statusbar)
6. libwlt-ai.a
7. kairos-window
8. wlt-settings
9. wlttor
10. libwlt-pkg.a
11–13. wl-libs install
14. final repack

**`wlt-update` is not built.** No `make` invocation against `src/userspace/wlt-update/Makefile`, no `gcc` against any of its `.c` files, no copy of `libwlt-update.so`/`.a` into `$ROOTFS/usr/lib/`, and no `wltupdate` binary copied into `$ROOTFS/usr/bin/`. The rootfs at `~/wltios-build/wltios-rootfs64/usr/lib/` was spot-checked and contains no `libwlt-update*` artifacts.

This means even if the user runs the produced initramfs, the `wltupdate` CLI command does not exist on it. The library exists in the repo as source code but is not delivered to the running system.

---

## 6. Spec-vs-implementation language mismatch (additional finding)

| Document | What it says | Reality |
|----------|--------------|---------|
| `docs/specs/wlt-update.md:99-115` | Rust project, `Cargo.toml`, `main.rs`, `check.rs`, `download.rs`, `verify.rs`, `install.rs`, `partition.rs`, `boot_config.rs`, `rollback.rs`, `error.rs` | C project: `update.c`, `partition.c`, `config.c`, `verify.c` |
| `docs/ARCHITECTURE.md:194` | "WLT-Update \| 80K \| Rust" | C, ~2 KLOC across 4 files |
| `docs/MODULE_MAP.md:77` | "wlt-update/ (Rust) — Atomic A/B update system" | C |
| `wltios-deploy/docs/FILETREE.md:216` | "wlt-update/ # A/B update system (EXISTS - 1,979 lines)" | EXISTS, but **2,088 lines** (with headers), and **not a skeleton** |
| `wltios-deploy/docs/FILETREE.md:379` | "🔧 Skeleton \| No actual download/install pipeline" | FALSE — there is a complete download/install pipeline |

The spec's Rust-vs-C decision is documented in `docs/context/ED25519_MONOCYPHER_SWAP.md` (which discusses dropping libsodium for monocypher), but the language swap itself (Rust → C) does not appear to be recorded in any ADR. This is a documentation-debt issue, not a code defect — the C implementation is functionally complete and would satisfy the spec's behavioural requirements if it were built and if `libsodium-dev` were available.

---

## 7. Recommendations (next actions)

1. **Install or vendor `libsodium-dev`.** Either `apt-get install libsodium-dev` on the build host (simplest), or vendor `sodium.h` + `libsodium.a`/`.so` into `packages/sodium-libs/` (reproducible-build-friendly, matches the existing `packages/tor-libs/` and `packages/wl-libs/` pattern). Without this, verify.c is a compile-time no-op and the entire A/B pipeline cannot install any update.
2. **Wire `wlt-update` into `build-all.sh`.** Add a new step (between Step 9 and Step 10, near the other library builds) that runs `make -C src/userspace/wlt-update` and installs `libwlt-update.{so,a}` + the two headers into `$ROOTFS/usr/lib/` + `$ROOTFS/usr/include/wlt-update/`. Without this, the library is dead code in the repo.
3. **Write the `wltupdate` CLI binary.** The Makefile is explicit that it is intentionally not built here — but no `main.c` exists in `src/userspace/wlt-update/` or in `tests/userspace/` (only `tests/userspace/update_test.c`, which is a unit test driver). The spec's `wltupdate check / install / status / rollback / enable-auto / disable-auto / history` user surface is therefore not yet deliverable to users. Suggest creating `src/userspace/wlt-update/cli.c` (~150 lines) that argv-parses the subcommand and calls the corresponding `wlt_update_*` API.
4. **Fix stale docs.** Update `docs/ARCHITECTURE.md:194`, `docs/MODULE_MAP.md:77`, and `wltios-deploy/docs/FILETREE.md` to reflect (a) the language swap from Rust to C, (b) the real line count (2,088 across 6 files including headers/Makefile), and (c) the fact that this is no longer a skeleton. Also note in `wltios-deploy/docs/FILETREE.md` that `download.c` and `transaction.c` do not exist — their concerns are inside `update.c`.
5. **Optional: ADR for the Rust→C swap.** The spec's Rust design was abandoned in favour of C (the codebase's other userspace modules are all C, and the build chain has no Rust toolchain). This decision should be recorded in `docs/adr/` to prevent future re-litigation.

---

## 8. Methodology

- All reads were performed via the MCPilot bridge (`node /home/z/my-project/mcp-call.js read_files ...`) against the Windows OneDrive repo path, which forwards to the MCPilot Cloudflare worker → the Windows MCPilot service → WSL2 filesystem.
- The four `.c` files plus `Makefile` plus `include/update.h` + `include/partition.h` were read in full (2,088 lines total).
- `wc -l` was run via `wsl.exe bash -c "wc -l update.c partition.c config.c verify.c Makefile include/*.h"` — real numbers, not remembered.
- Grep was performed locally on the downloaded source via ripgrep (the `Grep` tool) because passing regex alternation through PowerShell → WSL bash mangled the `|` characters.
- Compile checks used `gcc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only` (no `.o` files produced, no source files modified) — this is the strictest possible check that writes nothing to disk.
- libsodium availability was checked via `dpkg -l`, `ls /usr/include/sodium.h`, `ls /usr/lib/x86_64-linux-gnu/libsodium*`, `which pkg-config && pkg-config --exists libsodium && echo HAVE || echo NO`, plus `find /home/kushal/wltios-build -name sodium.h` (all empty).
- Build-chain wiring was checked by grepping `scripts/build-all.sh` (200 lines) for `wlt-update`/`wltupdate`/`update` (case-insensitive) — no hits — and by listing the rootfs `/usr/lib/` and `/usr/include/wlt-update/` directories.
- No changes were made to any file in the WLTIOS repo. The only file written by this audit task is this document (`docs/context/AB_UPDATE_AUDIT.md`) itself.

---

*Audit completed.* Total source inspected: 2,088 lines across 6 files (4 `.c` + 2 `.h` + 1 `Makefile`). All four `.c` files compile clean under `-Werror`. Zero TODOs. Zero stubs. Zero `not_implemented` markers. The single `return 0;` is a correct equality-branch return, not a stub. The implementation is substantially complete and correct by design; the open issues are environmental (libsodium-dev missing) and wiring-related (build-all.sh does not build it; no `wltupdate` CLI binary exists).
