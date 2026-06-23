# WLTIOS Integration Test Results

**Date:** 2026-06-23 (updated)
**Test:** WLTIOS custom initramfs v3 boot in QEMU + shell interaction test
**Result:** ✅ SUCCESS — shell accepts input and responds

## Boot Result: SUCCESS

### Initramfs contents (4 binaries, 1.3MB):
| Binary | Size | Role |
|--------|------|------|
| /sbin/init | 740K | wlt-init-minimal (PID 1) |
| /bin/wlt-shell | 736K | WLT-Shell (interactive) |
| /sbin/wlt-net | 713K | Network service stub |
| /sbin/wlt-crypto | 713K | Crypto service stub |

### Serial output (key markers):
```
=== WLTIOS v0.1.0 ===
seL4 -> Linux -> WLT-Init
========================================
[wlt-init] PID 1 starting
[wlt-init] Mounting filesystems... OK
[wlt-net] Network service initialized (stub)
[wlt-init] Starting wlt-net... OK
[wlt-crypto] Crypto service initialized (stub)
[wlt-init] Starting wlt-crypto... OK
[wlt-init] Starting wlt-shell...
WLTIOS Shell v1.0
Type 'help' for commands.
/wlt# help
Commands: ls, cat, echo, pwd, exit, help
/wlt# ls /
/wlt# pwd
/
/wlt# echo hello
/wlt#
```

### Shell interaction test results:
| Command | Sent? | Response? | Output |
|---------|-------|-----------|--------|
| help | ✅ | ✅ | "Commands: ls, cat, echo, pwd, exit, help" |
| ls / | ✅ | ⚠️ | (no output — busybox is 64-bit, can't run in 32-bit guest) |
| pwd | ✅ | ✅ | "/" |
| echo hello | ✅ | ⚠️ | (no "hello" output — shell wrapper limitation) |

### Service status:
| Service | Started? | Output |
|---------|----------|--------|
| wlt-net | ✅ OK | "[wlt-net] Network service initialized (stub)" |
| wlt-crypto | ✅ OK | "[wlt-crypto] Crypto service initialized (stub)" |
| wlt-shell | ✅ OK | "WLTIOS Shell v1.0" + prompt |

### Known issues:
1. **Null bytes in serial output** — 32-bit string literal alignment causes embedded \0 chars
   in some strings. Visible as garbled text. Fix: compile with -fno-merge-constants.
2. **ls / returns no output** — busybox is 64-bit, can't exec in 32-bit Linux guest.
   Fix: install 32-bit busybox or compile our own ls.
3. **echo hello** doesn't show output — simple shell wrapper doesn't handle all commands.
   Fix: use the real wlt-shell (currently falls back to wrapper due to compile warnings).

### What was NOT tested:
- Real network functionality (wlt-net is a stub)
- Real crypto functionality (wlt-crypto is a stub)
- File operations (ls/cat not working due to 64-bit busybox)

## Conclusion

Stage 2 wiring is progressing. The initramfs contains 4 binaries, all services start,
and the shell accepts interactive input via QEMU serial. `pwd` returns `/` confirming
the root filesystem is mounted. `help` lists available commands. Next steps: fix null
bytes, add 32-bit busybox for real file operations, test more shell commands.
