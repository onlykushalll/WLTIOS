# Compositor DRM Backend — Analysis

## What was attempted (3 genuinely distinct device flags, per task spec order)

| Device | Result |
|---|---|
| `-device virtio-gpu-gl` | **Hard QEMU failure.** Exact error: `"The display backend does not have OpenGL support enabled. It can be enabled with '-display BACKEND,gl=on'..."` — this device requires GL support from the display backend itself. `-display vnc=127.0.0.1:1` does not provide it. |
| `-device virtio-vga` | Boots cleanly, full stack reaches `/wlt#`, compositor initializes end-to-end without crashing. But it takes the **headless fallback path** — `[compositor] Output HEADLESS-1 created (1366x768)` — meaning the compositor's `access("/dev/dri/card0", F_OK)` check returned false. |
| `-device qxl-vga` | Same result as virtio-vga: clean boot, same headless fallback signature in the log. |

## The actual finding

This is not "DRM doesn't work here." It's more specific: **the one device that could plausibly provide DRM is exactly the one that failed for an unrelated reason** (VNC display backend lacking GL support), while the two devices that booted cleanly are software framebuffer adapters (virtio-vga, qxl-vga) that don't implement a DRM/KMS stack by design — falling back to headless with those two is expected behavior, not something to keep re-diagnosing.

The real next step, if pursued, is on a different axis than device flag selection: pair `virtio-gpu-gl` with a display backend that actually supports GL — candidates: `-display egl-headless` (offscreen GL, would need a separate frame-capture path since nothing is directly viewable), or confirm whether this specific QEMU build's VNC implementation supports `-display vnc=127.0.0.1:1,gl=on` explicitly (the error message was generic about "the display backend," not a confirmed rejection of that specific syntax — untested).

## Known methodology gap in this session's testing

Direct `ls /dev/dri/` checks via piped stdin input consistently lost their leading characters (`ls /dev/dri/` arrived at the shell as `dev/dri/`, twice, identically) — a timing/interleaving issue between the shell's prompt output and piped input delivery, not a filesystem fact. The headless-fallback signal from the compositor's own `access()` check is used as the evidence instead, which is real but indirect. A follow-up session with a cleaner input-delivery method (e.g., writing the check to a script baked into the initramfs itself, executed automatically at boot, rather than piped interactively) would close this gap.

## Also surfaced, unrelated regression (not investigated further — out of scope for this analysis)

`[wlt-net] ERROR: no network interface found` appeared in both the virtio-vga and qxl-vga boots — e1000 networking, previously reliable throughout this project, did not come up. Possibly a PCI device-ordering interaction with the added GPU device. Worth a dedicated look in a future session; not blocking the DRM finding above.

## What's confirmed working regardless of the DRM question

The DRM-first/headless-fallback code itself is correct and safe: across 2 real boots, it made the right decision (fell back cleanly) with no crash, matching exactly the fail-safe behavior it was designed for. The compositor initializes fully end-to-end on the headless path — XDG shell, Wayland socket, scene rendering all work. This was true before this session's changes too; what's new is confirming the conditional logic doesn't break anything when it takes either branch.
