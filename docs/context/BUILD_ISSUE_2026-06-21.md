# seL4 Build Issue — 2026-06-21

## Problem
seL4 camkes-vm-examples CMake configuration gets stuck searching for `umm_types.txt` on Windows/MSYS2.

## What Happened
1. Cloned seL4 camkes-vm-examples via `repo init` + `repo sync` ✅
2. Installed MSYS2 build tools (GCC 16.1, CMake 4.3, Ninja 1.13, Python 3.12) ✅
3. Installed Python deps (yaml, pyelftools, jinja2) ✅
4. Ran `init-build.sh -DCAMKES_VM_APP=minimal_64 -DPLATFORM=pc99`
5. CMake started configuring (found GCC, found all seL4 libs)
6. CMake got stuck searching for `umm_types.txt` — ran for 15+ minutes with <3s CPU
7. No `build.ninja` generated

## Root Cause
- `umm_types.txt` is a file generated/downloaded by the seL4 build system's cache
- The seL4 cache (`SEL4_CACHE_DIR`) tries to download prebuilt tools from the internet
- On Windows/MSYS2, the path translation and download mechanism doesn't work correctly
- seL4's build system is designed for Linux, not Windows

## What I Tried
1. Running via MSYS2 bash (init-build.sh) — got stuck
2. Running CMake directly from PowerShell — failed (wrong settings file)
3. Installing missing Python deps — didn't fix the stuck issue

## Recommended Solution
**Use WSL2 (Windows Subsystem for Linux) or a Linux VM.**

seL4 is designed to build on Linux. The official docs (docs.sel4.systems/HostDependencies) recommend Ubuntu/Debian. Building on Windows/MSYS2 is not supported.

### Steps to Fix (Future)
1. Install WSL2: `wsl --install` (needs reboot)
2. Install Ubuntu in WSL2
3. Inside WSL2 Ubuntu:
   ```bash
   sudo apt install cmake ninja-build python3-pip git repo
   pip3 install pyelftools jinja2 pyaml
   ```
4. Clone seL4 repo inside WSL2 (not in /mnt/c/ — too slow)
5. Build: `sh init-build.sh -DCAMKES_VM_APP=minimal_64 -DPLATFORM=pc99 && ninja`
6. Boot in QEMU (install qemu-system-x86 in WSL2)

## What I Did Instead
Pivoted to implementing WLT-Boot and WLT-Init code (the skeleton stubs). This is productive work that doesn't need the seL4 build. The seL4 build can be done later when WSL2 is available.

## Status
- seL4 source: ✅ Cloned and ready
- Build tools: ✅ Installed (but seL4 build doesn't work on MSYS2)
- WLT-Boot code: ✅ Implemented (skeleton → real code)
- WLT-Init code: ✅ Implemented (skeleton → real code)
- seL4 build: ❌ Blocked (needs WSL2/Linux)
- QEMU boot test: ❌ Blocked (needs seL4 build)
