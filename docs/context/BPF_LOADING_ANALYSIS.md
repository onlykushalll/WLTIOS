# BPF Loading Analysis — Why vmlinux.h Can't Be Extracted

## Problem
The Guardian's BPF program (`guardian.bpf.o`) fails to load with `bpf_object__open_file`. The BPF program was compiled with a minimal `guardian_vmlinux.h`. CO-RE (Compile Once - Run Everywhere) requires the real kernel's BTF types to resolve at load time.

## What Works
- BTF exists on the QEMU kernel: `/sys/kernel/btf/vmlinux` ✅
- `bpftool btf dump` generates 97,674-line vmlinux.h ✅
- eBPF engine initializes, loads 12 rules ✅
- eBPF engine reports "ready" (degraded mode) ✅

## What Doesn't Work
`bpf_object__open_file(path, NULL)` returns an error. The BPF object can't be loaded because it was compiled with minimal types that don't match the running kernel's BTF.

## Transfer Attempts (10 approaches, all failed)
1. **nc via 10.0.2.2** — WSL2 does not route QEMU guest→host traffic to WSL2 loopback
2. **Python listener on 0.0.0.0** — No connection received from guest
3. **Python listener on 127.0.0.1** — No connection received from guest
4. **virtio-9p** — Guest lacks `mount` and `modprobe` commands (minimal initramfs)
5. **virtio-serial** — Guest lacks virtio-serial kernel driver
6. **USB storage** — Guest lacks USB storage driver
7. **virtio-blk disk** — Guest lacks virtio-blk driver (`/dev/vda` not found)
8. **QEMU chardev pipe** — Captured wrong serial port (ttyS0 instead of ttyS1)
9. **Disk image raw write** — No data appeared on disk after `cat > /dev/vda`
10. **nc via hostfwd** — `hostfwd` forwards host→guest, not guest→host

## Root Cause
QEMU user-mode networking in WSL2 does not route `10.0.2.2` (guest gateway) to the WSL2 host's loopback interface. The guest CAN reach the internet (Tor bootstraps 100%), but cannot reach services on the WSL2 host. This is a known WSL2 networking limitation.

## What Would Fix This
1. **Run QEMU with `-netdev tap`** instead of `user` — requires root/sudo (not available)
2. **Use a kernel with virtio-9p built-in** — not as a loadable module
3. **Run QEMU on Windows directly** — not inside WSL2
4. **Use a different kernel** that has virtio-blk/virtio-serial built-in
5. **Bundle bpftool + clang in the guest initramfs** and compile the BPF program inside the guest

## Impact
The Guardian's eBPF enforcement tier is in degraded mode. The 12 eBPF rules are loaded but no BPF programs are attached to the kernel. This means:
- No execve monitoring (but YARA signature scanning works via socket API)
- No connect() interception (but route-based kill switch works)
- No ptrace blocking
- No fork-bomb rate limiting
- No inline SIGKILL

The YARA, FIM, anomaly, canary, ransomware, and net engines all work. The kill switch works (route-based). The gap is specifically the eBPF inline enforcement.
