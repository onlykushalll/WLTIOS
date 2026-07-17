# Tor Runtime Libraries

These libraries are required by the Tor binary in the rootfs but are
not always present in the minimal initramfs. They are vendored here
so the build is reproducible without depending on any specific WSL2
install's system packages.

## Files

| File | Source | Why needed |
|------|--------|-----------|
| liblzma.so.5 | Ubuntu 24.04 liblzma5 package | Tor decompression |
| libzstd.so.1.5.7 | Ubuntu 24.04 libzstd1 package | Tor compression (must be 64-bit ELF) |
| libevent-2.1.so.7.0.1 | Alpine tor-staging (extracted from tor apk) | Tor event loop |

## Usage

`build-all.sh` copies these into the rootfs `/lib/` directory and
creates the necessary symlinks (libzstd.so.1, libevent-2.1.so.7).
