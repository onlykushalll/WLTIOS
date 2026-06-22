#!/bin/bash
echo "=== Installing wlroots dependencies ==="
sudo apt install -y \
  libwayland-dev libwayland-server0 \
  libxkbcommon-dev libpixman-1-dev \
  libdrm-dev libgbm-dev libegl1-mesa-dev \
  libgles2-mesa-dev libseat-dev \
  libinput-dev libudev-dev \
  libxcb-dri3-dev libxcb-present-dev \
  pkg-config meson ninja-build 2>&1 | tail -5
echo ""

echo "=== Cloning wlroots 0.17.4 ==="
cd ~
if [ -d wlroots ]; then
    echo "wlroots already cloned"
else
    git clone --depth=1 --branch=0.17.4 https://gitlab.freedesktop.org/wlroots/wlroots.git 2>&1 | tail -3
fi
echo ""

echo "=== Building wlroots ==="
cd ~/wlroots
meson setup build --prefix=/usr/local 2>&1 | tail -10
echo ""
ninja -C build 2>&1 | tail -5
echo ""
echo "=== Installing wlroots ==="
sudo ninja -C build install 2>&1 | tail -5
sudo ldconfig
echo ""
echo "=== Verifying ==="
pkg-config --modversion wlroots 2>&1
