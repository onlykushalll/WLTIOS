# WLTIOS — UI, Compositor, Apps, and Design System (Consolidated Reference)

> **Supersedes:** `docs/specs/wlt-compositor.md`,
> `docs/specs/wlt-compositor-notes.md`, `docs/specs/wlt-shell.md`,
> `docs/specs/wlt-terminal.md`, `docs/specs/wlt-filemanager.md`,
> `docs/specs/wlt-settings.md`, `docs/specs/wlt-design-system.md`, and the
> compositor sections of `docs/context/COMPOSITOR_DRM_ANALYSIS.md`.
> Archived under `_ARCHIVED_2026-07-29`.

---

## 1. Compositor - real, and further along than the original spec's "Phase 2, Month 7-8" framing suggests

Built on wlroots 0.18.2 (not a from-scratch Wayland implementation, despite
some earlier marketing-style docs saying "built from scratch"). Confirmed
via the CHANGELOG and re-verified against source in this pass:

- Real `wlr_backend_autocreate`, `wlr_renderer_autocreate`,
  `wlr_allocator_autocreate`, `wlr_scene_create`, `wlr_output_layout_create`,
  `wlr_seat_create` initialization (not stub structs).
- Multi-window desktop with resize/minimize/maximize/drag/Alt+Tab -
  verified via actual QEMU screendump + VLM (vision-model) pixel
  description, not just "it compiles" (2026-07-12 session).
- **Rendering backend:** currently pixman/software rendering by default in
  this QEMU environment, because virtio-gpu here doesn't expose a working
  EGL/GL context. A conditional GLES2 renderer-selection path
  (`c30197a`, 2026-07-22) and a real (not stubbed) dual-pass separable
  Gaussian backdrop-blur shader (`b2d9b08`, 2026-07-23) exist as the start
  of the glassmorphism/GPU track. Whether GLES2 actually engages at
  runtime here, vs. falling back to pixman, was not re-confirmed in this
  pass.
- **Window snapping** (`99b69b3`) and **right-click context menus**
  (`816cb7c`, FR-GUI-11) landed 2026-07-26 and 2026-07-28 respectively -
  both later than every archived doc that described the compositor as
  basic/incomplete in this area.
- Build health: a real `-Werror`/Makefile fix (`9c30d8e`, 2026-07-23) was
  needed to keep the compositor building at all - a reminder that "it
  built last week" doesn't mean "it builds now" without re-checking.

`docs/context/COMPOSITOR_DRM_ANALYSIS_ARCHIVED_2026-07-29.md` (kept content,
folded in here) is the source for the DRM/virtio-gpu limitation described
above; its analysis reads as genuine investigation (specific error
messages, specific fallback behavior) rather than assumption.

## 2. wlt-shell and apps

Real, per the CHANGELOG/commit history and the orphaned-code audit
(`wlt-ai` and other modules confirmed real by line-count+stub-check, not
self-report): terminal, file manager, launcher, statusbar, settings, and
the Kairos sidebar all render simultaneously on the desktop. Recent app
additions confirmed via `git log` (not re-verified line-by-line in this
pass, but each has a specific, plausible commit message and an FR-ticket
reference, which is a better signal than a bare "done" claim):

| App | Commit | Date |
|---|---|---|
| Text editor (FR-APP-03) | `529a104` | 2026-07-26 |
| Task manager (FR-APP-05) | `25014b7` | 2026-07-26 |
| Screenshot tool (FR-APP-06) | `cb3daab` | 2026-07-26 |
| Tor Browser v1.0 (FR-APP-10) | `5bf6a83` | 2026-07-26 |
| SOCKS5 Tor proxy support for `wlt_http` (real fetch, FR-APP-10) | `e593468` | 2026-07-27 |

These were not individually re-opened and read in this consolidation pass -
they are recorded here as "git history shows this landed," which is a
weaker but still meaningful signal than the source-level verification done
elsewhere in this document set. Re-verify before treating any single one as
feature-complete.

## 3. wlt-settings, wlt-terminal, wlt-filemanager (original specs)

No dedicated re-audit of these three was located in the project's own
history or performed in this pass beyond what's implied by the general app
list above. Treat their specs (folded content: standard categories -
network/display/sound/security/privacy/accounts/updates/apps/system/about
for Settings; PTY+ANSI+scrollback for Terminal; Tor-download/per-file-
encryption/secure-delete/privacy-badges/AI-search for File Manager) as
target design, and check current source directly before making status
claims about them.

## 4. Design system (glassmorphism / "Liquid Glass")

This is a genuine forward-looking **technical translation document**, not a
stale status claim, so its content is summarized here rather than marked
as a conflict:

- **Glassmorphism** (blur + translucency + gradient border + SDF-rounded
  corners) is the cheap, default effect for most surfaces - directly
  translatable to GLES2, no research risk.
- **"Liquid Glass"** (Apple WWDC 2025 style - SDF-driven refraction,
  edge-only chromatic aberration, specular highlight from an SDF-derived
  normal, layered noise+blur) is deliberately reserved for 1-2
  high-visibility surfaces (the Kairos sidebar is the named candidate)
  because it's meaningfully more expensive per-pixel.
- Build order specified: glassmorphism first (reuses the blur pass already
  landed, `b2d9b08`), apply to the highest-visibility surface, measure real
  frame-time cost, only then attempt Liquid Glass on one surface.
- As of this consolidation, the blur shader is real and landed; whether
  glassmorphism or Liquid Glass has been applied to any actual surface
  (vs. just the shader existing) was not confirmed - re-check the
  compositor's render path for actual usage before claiming the visual
  effect is live.

## 5. Open items

- Confirm GLES2 vs. pixman at actual runtime in the current QEMU config
- Re-audit wlt-settings/wlt-terminal/wlt-filemanager completeness
  (no existing audit covers them at the rigor applied to wlt-update)
- Confirm whether glassmorphism/Liquid Glass shaders are wired to any live
  surface or exist only as standalone shader code
