/**
 * @file blur.h
 * @brief Backdrop blur (dual-pass Gaussian) for the compositor
 * @purpose GPU Section B — one real blur pass, proven.
 *
 * Implements a standard dual-pass (horizontal + vertical) Gaussian
 * backdrop blur using GLES2 shaders. This is the textbook technique
 * (not a novel algorithm) used by Hyprland, KWin, and Mutter.
 *
 * The blur is applied to a region of the composited scene as a proof
 * of concept (PRD Section 7 Phase 2). It is NOT a per-surface blur
 * yet (Phase 4) — it demonstrates the shader pipeline works end-to-end
 * through the wlroots GLES2 renderer.
 *
 * @dependencies: GLES2, EGL, wlroots 0.18 gles2 renderer
 * @spec: docs/specs/wlt-design-system.md (glassmorphism / blur rules)
 */
#ifndef WLT_BLUR_H
#define WLT_BLUR_H

#include <stdbool.h>
#include <GLES2/gl2.h>

typedef struct wlt_blur wlt_blur_t;

/* Create a blur shader program. radius_px is the Gaussian sigma in pixels
 * (typical: 8-20 for backdrop blur). Returns NULL on shader compile/link
 * failure or if GLES2 is not the active renderer. */
wlt_blur_t *wlt_blur_create(int radius_px);

/* Apply the blur to the currently-bound framebuffer's content, rendering
 * the blurred result into the same region. This is the dual-pass:
 *   pass 1: horizontal Gaussian -> intermediate FBO
 *   pass 2: vertical Gaussian -> back to the target
 * x/y/w/h define the screen region to blur (origin bottom-left, GLES).
 * screen_w/h are the full output dimensions (for texture coord setup). */
void wlt_blur_apply(wlt_blur_t *b, int x, int y, int w, int h,
                    int screen_w, int screen_h);

/* Free shader resources. Safe to call with NULL. */
void wlt_blur_destroy(wlt_blur_t *b);

#endif /* WLT_BLUR_H */
