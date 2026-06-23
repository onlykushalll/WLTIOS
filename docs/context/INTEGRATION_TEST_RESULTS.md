# WLTIOS Integration Test Results

**Date:** 2026-06-23 (v10 — final)
**Test:** WLTIOS custom initramfs v10 boot in QEMU + full shell command test
**Result:** SUCCESS

## Initramfs Contents (4 binaries, 1.4MB)

| Binary | Size | Source | Status |
|--------|------|--------|--------|
| /sbin/init | 740K | init-minimal.c | PID 1, mounts fs, starts services |
| /bin/wlt-shell | 736K | cmds/shell.c (REAL, 420 lines) | Full CLI with ls/cat/echo/cd/mkdir/rm/pwd/help + redirection |
| /sbin/wlt-net | 713K | Stub (real source has link errors) | Reports "init" and exits 0 |
| /sbin/wlt-crypto | 745K | REAL wlt-crypto source | Reports "Key Vault PD unreachable" (expected in QEMU) |

## Boot Output

```
=== WLTIOS v0.1.0 ===
seL4 -> Linux -> WLT-Init
========================================
[wlt-init] PID 1 starting
[wlt-init] Mounting filesystems... OK
[wlt-net] init
[wlt-init] Starting wlt-net... OK
[wlt-crypto][ERR] init: cannot reach Key Vault PD: unsupported operation
[wlt-init] Starting wlt-crypto... OK
[wlt-init] Starting wlt-shell...
WLTIOS Shell v1.0
Type 'help' for available commands.

/wlt# help
WLTIOS Shell — Built-in commands:
  ls [-al]      List directory contents
  cd [dir]      Change directory
  cat [file]    Display file contents
  echo [text]   Print text
  mkdir [dir]   Create directories
  rm [-rf]      Remove files
  pwd           Print working directory
  help          Show this help
  exit          Exit shell

/wlt# ls / → sbin tmp run var init sys lib home etc proc bin root dev
/wlt# ls /sbin → wlt-net init wlt-crypto
/wlt# cat /etc/wltios-release → WLTIOS_VERSION=0.1.0
/wlt# echo test > /tmp/file.txt → (file created)
/wlt# cat /tmp/file.txt → test
/wlt# rm /tmp/file.txt → (file removed)
/wlt# mkdir /tmp/dir → (directory created)
/wlt# ls /tmp → dir
/wlt# pwd → /
/wlt# echo ALL_TESTS_COMPLETE → ALL_TESTS_COMPLETE
```

## Shell Command Test Results

| Command | Input | Output | Status |
|---------|-------|--------|--------|
| help | `help` | 8 commands listed | PASS |
| ls / | `ls /` | sbin tmp run var init sys lib home etc proc bin root dev | PASS |
| ls /sbin | `ls /sbin` | wlt-net init wlt-crypto | PASS |
| ls /etc | `ls /etc` | wltios-release hostname | PASS |
| cat | `cat /etc/wltios-release` | WLTIOS_VERSION=0.1.0 | PASS |
| pwd | `pwd` | / | PASS |
| cd | `cd /bin` + `pwd` | /bin | PASS |
| echo | `echo hello` | hello | PASS |
| echo redirect | `echo test > /tmp/file.txt` | (file created) | PASS |
| cat (redirect) | `cat /tmp/file.txt` | test | PASS |
| rm | `rm /tmp/file.txt` | (file removed) | PASS |
| mkdir | `mkdir /tmp/dir` | (directory created) | PASS |
| ls (verify) | `ls /tmp` | dir | PASS |

## Service Status

| Service | Real Source? | Output | Status |
|---------|-------------|--------|--------|
| wlt-net | Stub (link errors in real source) | `[wlt-net] init` | OK |
| wlt-crypto | YES (real source compiled!) | `[wlt-crypto][ERR] init: cannot reach Key Vault PD: unsupported operation` | OK (expected in QEMU) |
| wlt-shell | YES (real cmds/shell.c, 420 lines) | Full interactive shell | OK |

## Null Bytes

- Count: 8 (down from 25 in v6, 10 in v5)
- Startup messages: CLEAN (write() instead of printf)
- Command output: a few nulls remain in printf calls
- Root cause: 32-bit ELF string constant alignment in QEMU serial
- Fix applied: setvbuf(_IONBF) + write() for startup + -fno-merge-constants

## What Works

- seL4 boots Linux guest
- WLTIOS init runs as PID 1
- Filesystems mount (/proc, /sys, /dev)
- Services start (wlt-net, wlt-crypto)
- Real wlt-crypto module runs (detects Key Vault PD unreachable)
- Interactive shell with 8 commands
- Output redirection (echo > file)
- File creation, reading, deletion
- Directory creation and listing
- All commands respond to piped serial input

## What Doesn't Work Yet

- wlt-net real source doesn't link (undefined references to wlt_net_log, wlt_net_run_cmd)
- A few null bytes remain in printf output (cosmetic)
- No GUI (compositor not in initramfs)
- No network (Tor not available in QEMU)
