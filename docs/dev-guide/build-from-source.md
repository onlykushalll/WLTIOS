# WLTIOS Developer Guide: Building from Source

## Prerequisites

### Build Environment
- WSL2 Ubuntu 24.04+ (recommended) or native Linux
- GCC 13+ (GCC 15 works with werror=false for wlroots)
- CMake 4.0+
- Ninja 1.13+
- Python 3.12+
- QEMU 10.0+ (for testing)

### Install Build Dependencies
```bash
sudo apt update
sudo apt install -y \
    build-essential gcc-multilib g++-multilib \
    cmake ninja-build python3 python3-pip \
    qemu-system-x86 qemu-utils \
    git wget curl \
    libwayland-dev libwayland-server0 \
    libxkbcommon-dev libpixman-1-dev \
    libdrm-dev libgbm-dev libegl1-mesa-dev \
    libgles2-mesa-dev libseat-dev \
    libinput-dev libudev-dev \
    libxcb-dri3-dev libxcb-present-dev \
    pkg-config meson \
    wayland-protocols libdisplay-info-dev libliftoff-dev
```

### Build wlroots from Source
```bash
cd ~
git clone --depth=1 --branch=0.18.2 https://gitlab.freedesktop.org/wlroots/wlroots.git
cd wlroots
meson setup build --prefix=/usr/local -Dwarning_level=1 -Dwerror=false
ninja -C build
sudo ninja -C build install
sudo ldconfig

# Add to PKG_CONFIG_PATH
echo 'export PKG_CONFIG_PATH=/usr/local/lib/x86_64-linux-gnu/pkgconfig:$PKG_CONFIG_PATH' >> ~/.bashrc
source ~/.bashrc

# Verify
pkg-config --modversion wlroots-0.18  # Should return 0.18.2
```

## Building WLTIOS

### 1. Clone the Repository
```bash
git clone https://github.com/onlykushalll/WLTIOS.git
cd WLTIOS
```

### 2. Build seL4 + Linux Guest (32-bit for QEMU testing)
```bash
# Clone seL4 camkes-vm-examples
mkdir ~/wltios-build && cd ~/wltios-build
repo init -u https://github.com/seL4/camkes-vm-examples.git
repo sync

# Configure for 32-bit (works in WSL2 QEMU)
mkdir build-minimal-32 && cd build-minimal-32
../init-build.sh -DCAMKES_VM_APP=minimal -DCMAKE_BUILD_TYPE=Release
ninja -j$(nproc)
```

### 3. Build WLT-Init-Stub (for QEMU boot testing)
```bash
gcc -m32 -Wall -Wextra -Werror -static \
    src/userspace/wlt-init/wlt-init-stub.c \
    -o wlt-init-stub-32
```

### 4. Inject into rootfs + Boot in QEMU
See `docs/context/QEMU_BOOT_INSTRUCTIONS.md` for the full guide.

### 5. Run Unit Tests
```bash
make -f tests/Makefile test
```

### 6. Run Module Tests (all modules, real source)
```bash
for mod in compositor shell pkg net crypto sandbox permissions settings tor_mgr update filemanager; do
    dir="src/userspace/wlt-${mod}"
    [ "$mod" = "tor_mgr" ] && dir="src/userspace/wlt-tor-mgr"
    
    gcc -Wall -Wextra -Werror -std=gnu11 -DWLT_TESTING -D_GNU_SOURCE -DWLR_USE_UNSTABLE \
        -include stdarg.h -include stdint.h -include string.h -include stdlib.h \
        -include stdio.h -include sys/types.h -include unistd.h -include errno.h \
        $(pkg-config --cflags wlroots-0.18 wayland-server 2>/dev/null) \
        -o /tmp/wlt-test-${mod} \
        tests/userspace/${mod}_test.c \
        $(find "$dir" -maxdepth 1 -name "*.c" | tr '\n' ' ') \
        -I"$dir/include" \
        $(pkg-config --libs wlroots-0.18 wayland-server 2>/dev/null) -lm
    
    /tmp/wlt-test-${mod}
done
```

## Architecture Overview

```
seL4 (verified microkernel)
├── Linux Guest VM (general apps, GUI, network)
├── Tor VM (isolated Tor daemon)
├── Firewall PD (packet filtering, kill switch)
├── Key Vault PD (encryption keys, crypto operations)
└── Boot Verifier PD (integrity verification)
```

## Module Map

See `docs/MODULE_MAP.md` for the complete list of modules and their status.

## Coding Style

See `docs/CODING_STYLE.md`. Key rules:
- Allman brace style
- No malloc in PID 1 (WLT-Init) — use static buffers
- Module header comment on every file
- -Wall -Wextra -Werror — no warnings allowed
- No v1/v2/v3 files — modify existing files

## Adding a New Module

1. Write a spec in `docs/specs/your-module.md`
2. Get spec approved (submit as PR for review)
3. Create `src/userspace/wlt-yourmodule/` with:
   - `include/wlt-yourmodule.h` (public API)
   - `yourmodule.c` (implementation)
   - `Makefile` (follows wlt-compositor pattern)
4. Write tests in `tests/userspace/yourmodule_test.c`
5. Add to `docs/MODULE_MAP.md`
6. Add to `config/services/wlt-yourmodule.service`
7. Commit: `git commit -m "[module] add: wlt-yourmodule — <description>"`
8. Run tests: `make -f tests/Makefile test`
