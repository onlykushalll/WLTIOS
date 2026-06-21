# Build Options Comparison — seL4 on Your PC

> **Date:** 2026-06-21
> **Question:** How to build seL4 without breaking your Windows setup

---

## The Problem

seL4's build system is designed for **Linux**. On Windows/MSYS2, CMake gets stuck downloading cache files (`umm_types.txt`). We need a Linux environment to build seL4.

## Option 1: WSL2 (Windows Subsystem for Linux) — RECOMMENDED

### What It Is
WSL2 is a Microsoft-official Linux environment that runs **inside Windows**. It's like having Linux in a window — NOT a separate boot partition.

### Safety
| Concern | Reality |
|---------|---------|
| Boot menu? | ❌ No boot menu. Windows boots normally. |
| Disk partition? | ❌ No partitioning. WSL2 uses a virtual disk file (`.vhdx`) |
| Bootloader changes? | ❌ None. Your Windows bootloader is untouched. |
| Can uninstall? | ✅ Yes, like any app (`wsl --unregister Ubuntu`) |
| Affects Windows? | ❌ No. Completely isolated. |

### What It Does
- Enables Windows' built-in virtualization feature (Hyper-V)
- Runs a real Linux kernel inside Windows
- You get a Linux terminal — can run `cmake`, `ninja`, `gcc` natively
- Files stay in a virtual disk, not on your Windows filesystem

### Installation
```powershell
# In PowerShell as admin:
wsl --install -d Ubuntu
# Reboot required (one time, to enable virtualization)
# After reboot, Ubuntu opens automatically
# Set username/password (doesn't need to match Windows)
```

### Pros
- ✅ seL4 builds natively on Linux (no MSYS2 issues)
- ✅ wlroots + Wayland build natively
- ✅ QEMU works inside WSL2
- ✅ Free, official Microsoft feature
- ✅ Can uninstall anytime

### Cons
- ⚠️ One reboot required (to enable virtualization)
- ⚠️ Uses ~1-2 GB RAM when running
- ⚠️ You might be prompted to enable "Virtual Machine Platform" Windows feature

### My Assessment
**This is the safest option.** Microsoft built WSL2 specifically for developers who need Linux tools without dual-booting. Your Windows installation is completely untouched. If you don't like it, `wsl --uninstall` removes everything.

---

## Option 2: VirtualBox VM (You already have VirtualBox)

### What It Is
Install Ubuntu in VirtualBox — a full Linux VM in a window.

### Safety
| Concern | Reality |
|---------|---------|
| Boot menu? | ❌ No. VirtualBox boots inside Windows. |
| Disk partition? | ❌ No. VM uses a virtual disk file. |
| Bootloader changes? | ❌ None. |
| Can uninstall? | ✅ Delete the VM, done. |
| Affects Windows? | ❌ No. |

### Installation
1. Download Ubuntu ISO (https://ubuntu.com/download)
2. Create new VM in VirtualBox (4GB RAM, 30GB disk)
3. Install Ubuntu in the VM
4. Install build tools inside Ubuntu

### Pros
- ✅ Fully isolated (VM)
- ✅ No Windows changes at all
- ✅ You already have VirtualBox

### Cons
- ⚠️ Slower than WSL2 (full VM overhead)
- ⚠️ Uses 4GB RAM when running (you have 8GB total)
- ⚠️ More setup (install Ubuntu, configure, etc.)
- ⚠️ Nested virtualization for QEMU might be tricky

### My Assessment
Works, but WSL2 is better (faster, lighter, more integrated).

---

## Option 3: Cloud VPS (Zero impact on your PC)

### What It Is
Rent a Linux server in the cloud. Build seL4 there. Download the .iso to test in QEMU.

### Safety
| Concern | Reality |
|---------|---------|
| Boot menu? | ❌ No. Nothing on your PC. |
| Disk partition? | ❌ No. |
| Anything on your PC? | ❌ Only the downloaded .iso file. |
| Can stop? | ✅ Cancel the VPS subscription. |

### Providers
- DigitalOcean: $6/month (1GB RAM — might not be enough)
- Hetzner: $4/month (2GB RAM — better)
- AWS EC2 t3.micro: Free tier for 1 year

### Pros
- ✅ Zero impact on your PC
- ✅ Always available (don't need your PC on)
- ✅ Can scale up if needed

### Cons
- ⚠️ Costs money ($4-12/month)
- ⚠️ Need to manage SSH keys
- ⚠️ Can't test GUI (QEMU display) — only serial console
- ⚠️ Download .iso to your PC for QEMU testing

### My Assessment
Good if you don't want anything on your PC. But WSL2 is free and better.

---

## Option 4: Stay on Windows (Limited — What We're Doing Now)

### What We Can Do
- ✅ Write all C code (WLT-Boot, WLT-Init — done)
- ✅ Write all Rust code (WLT-Pkg, WLT-Net, etc.)
- ✅ Write all documentation
- ✅ Write all specs
- ✅ Write tests
- ✅ Git commits + GitHub

### What We Cannot Do
- ❌ Build seL4 (stuck on MSYS2)
- ❌ Build wlroots (needs Linux)
- ❌ Test in QEMU (needs seL4 build first)

### My Assessment
We can write all the code on Windows. The actual building + testing needs Linux. So we'd be writing code blind — no way to verify it compiles or works.

---

## My Recommendation

**Use WSL2.** Here's why:

1. **Safest** — doesn't touch your Windows at all
2. **Free** — built into Windows 10/11
3. **Fast** — near-native Linux performance
4. **Reversible** — `wsl --uninstall` removes everything
5. **Required** — seL4 and wlroots both need Linux

### What I'd Do (If You Approve WSL2)

1. `wsl --install -d Ubuntu` (you run this, one reboot)
2. Inside WSL2 Ubuntu:
   - Install build tools (cmake, ninja, gcc, python)
   - Clone seL4 repo (inside WSL2, not /mnt/c/)
   - Build seL4: `sh init-build.sh && ninja`
   - Boot in QEMU
3. Verify seL4 + Linux boots
4. Commit build results to GitHub
5. Continue with WLT-Boot implementation

### What I Will NOT Do

- ❌ Install WSL2 without your explicit permission
- ❌ Touch your boot settings
- ❌ Partition your disk
- ❌ Install a dual-boot
- ❌ Modify your UEFI/BIOS

---

## Your Decision

**Tell me which option:**

1. **"Install WSL2"** — I'll guide you through the one command, you reboot, then I continue building seL4
2. **"Use VirtualBox"** — I'll help you set up an Ubuntu VM
3. **"Cloud VPS"** — I'll help you set up a cloud server
4. **"Stay on Windows"** — I continue writing code, build later
5. **"Not sure, tell me more"** — I'll answer any questions

**I will not proceed with any option that touches your system without your explicit "yes, do it."**

---

*Your PC. Your rules. I'm just the builder.*
