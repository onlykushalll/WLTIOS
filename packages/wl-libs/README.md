# Wayland + wlroots Runtime Libraries

Shared libraries required by the compositor (wlt-compositor) and all
Wayland client binaries (wlt-terminal, wlt-filemanager, wlt-launcher,
wlt-statusbar, kairos-window, wlt-settings).

These are captured via `ldd` against the compiled binaries on the
WSL2 build host. The compositor alone needs ~30 shared libs including
libwlroots, libwayland, libdrm, libpixman, libxkbcommon, libEGL, etc.

## Usage

`build-all.sh` Step 13 copies these into the rootfs.
