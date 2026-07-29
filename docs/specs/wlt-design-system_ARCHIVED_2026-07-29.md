# WLTIOS Design System — Glassmorphism & Liquid Glass

**Status:** Research complete, spec ready for implementation. Written ahead
of the GPU/UI track reaching this section, so shader work starts from a
correct technical translation instead of reverse-engineering CSS under time
pressure.

**Why this exists:** the reference material for this style (CSS
`backdrop-filter`, SVG `feDisplacementMap`/`feSpecularLighting` filters) is
web-rendering technology. WLTIOS's compositor renders natively via
GLES2/pixman — there is no backdrop-filter or SVG filter pipeline sitting
there waiting to be used. Every effect below needs a GLSL translation and a
rendering pipeline that supports it (sampling the framebuffer behind a
surface, generating noise, computing per-pixel lighting). This document is
that translation.

---

## 1. Two distinct effects — don't conflate them

**Glassmorphism** (the CSS example) is simple: blur + translucency + a
subtle inset highlight + a soft border. It's a 2D compositing effect —
blur the backdrop, blend a translucent tint over it, add a 1px gradient
border for a "light catching an edge" impression. This is the baseline for
most surfaces (windows, panels, the taskbar).

**Liquid Glass** (Apple's WWDC 2025 design language, the SVG/React example)
is a different, more expensive effect: the surface actually *refracts* and
*distorts* what's behind it, the way real glass bends light, plus specular
highlights that respond to a simulated light source and (in Apple's real
implementation) to device motion/cursor position. This is a true optical
simulation, not just a blur. Apple's own materials describe it as combining
"the optical properties of glass with a sense of fluidity" — reflective
highlights, fluid dynamic response, and depth.

**Design rule: use Liquid Glass sparingly.** Apple applies it to focused,
high-attention elements — controls, floating panels, the elements meant to
feel alive under the finger/cursor — not to every surface. Glassmorphism
(cheap, blur-based) is the default for most windows and panels; Liquid Glass
(expensive, refraction-based) is reserved for 1-2 signature surfaces where
it earns its cost. Candidates: the Kairos sidebar (the AI assistant is
exactly the kind of "always alive, always responsive" surface Apple uses
this effect for) and the app launcher/dock.

---

## 2. Glassmorphism — implementation (GLES2, cheap, use everywhere)

Direct translation of the CSS reference, no exotic technique needed:

1. **Backdrop blur:** a two-pass separable Gaussian blur (horizontal pass,
   then vertical pass) sampling the compositor's existing framebuffer
   region behind the surface. This is the same blur already scoped for
   the GPU track's Section B — nothing new here, just confirming the CSS
   `backdrop-filter: blur(20px)` maps directly onto that shader.
2. **Tint:** blend a low-alpha white/black overlay (`rgba(255,255,255,0.14)`
   in the reference) over the blurred backdrop — a single `mix()` in the
   fragment shader.
3. **Border + inset highlights:** the CSS reference's `border:
   1px solid rgba(255,255,255,0.3)` plus inset box-shadows simulating a
   top-edge light catch. In GLES2 terms: render a 1px border with a
   vertical gradient (brighter at the top edge, matching the CSS
   `::before` pseudo-element's top-highlight gradient), and an inner
   shadow gradient along the top/left edges. This is straightforward
   procedural gradient math in the fragment shader, not a shader research
   problem — the CSS reference is a complete enough spec for this part.
4. **Corner radius:** GLES2 doesn't have CSS `border-radius` for free —
   use a signed-distance-field (SDF) rounded-rect test in the fragment
   shader (`sdRoundedBox` — this is a standard, widely-documented
   technique, e.g. Inigo Quilez's SDF function library) to discard/blend
   pixels outside the rounded shape and to drive the border-gradient math
   near the edges.

This alone covers most of what the user's `.glass-card` CSS reference does,
translated 1:1. No research risk here — implement directly.

## 3. Liquid Glass — implementation (GLES2, expensive, use on 1-2 surfaces)

Based on Apple's own described principles (reflective highlights that
"dynamically respond to movement," fluid dynamics, depth) and the closest
real native (non-web) reference implementation found
(`OverShifted/LiquidGlass`, OpenGL+GLSL, MIT-licensed, ~110 stars — its
own README: *"renders a squircle with a shader that approximates a signed
distance field to emulate refraction. With some blur, noise and a bit of
glow, it ends up looking somewhat like glass"*), plus a technical
breakdown (Aghajari, "Liquid Glass: iOS Effect Explanation") that names
five components:

1. **Shape via SDF, not a rectangle.** Use a rounded-rect or "squircle" SDF
   (same primitive as glassmorphism's corner radius, reused here). The SDF
   gives a signed distance to the edge at every pixel — this distance
   value is the input to everything else below.
2. **Refraction via SDF-gradient-driven UV distortion.** This is the
   translation of the SVG reference's `feDisplacementMap`: instead of
   generating a noise texture and displacing by it (the SVG/web approach),
   drive the displacement from the SDF's gradient near the edges — pixels
   near the boundary get their backdrop-sample UV coordinate offset
   proportional to `(1 - distance_from_edge)` and the SDF's normal
   direction. This produces the "lensing" look (content bends toward/away
   from the edges) without needing a separate noise-texture pass, and is
   cheaper than the web version's `feTurbulence`. A small amount of actual
   noise (see below) can be layered on top for organic irregularity, but
   the *primary* distortion driver should be the SDF gradient, not noise —
   noise-only distortion (what the user's SVG reference does) looks more
   like frosted texture than lensing; the SDF-driven version is what
   produces Apple's actual "glass bending light" look.
3. **Edge-only chromatic aberration.** Sample the backdrop three times
   (once per R/G/B channel) with a slightly different UV offset per
   channel, but only near the edge (weight this by the same
   distance-from-edge term as the refraction) — real glass shows color
   fringing at its bent edges, not across a flat center. This is a small,
   cheap addition once refraction sampling exists (3 texture samples
   instead of 1, only in the edge band).
4. **Specular highlight.** The SVG reference's
   `feSpecularLighting`+`feDistantLight` approximates a directional light
   hitting a bump map (the noise). Translate this as: compute a
   pseudo-normal from the SDF gradient (the edge curvature already gives a
   usable normal direction without needing the noise map for this part),
   then a standard Blinn-Phong specular term against a fixed (or, later,
   cursor/focus-driven) light direction. This produces the moving
   highlight streak along the glass's curved edge.
5. **Subtle noise + blur, layered last.** A small amount of procedural
   noise (simplex/value noise, cheap in GLSL) breaks up perfect
   mathematical smoothness so it doesn't look like a flat vector shape;
   the same two-pass Gaussian blur from Section 2 softens the composited
   result. These are finishing touches, not the core effect — get 1-4
   right first.

**Performance note:** this is meaningfully more expensive than
glassmorphism (multiple backdrop samples per pixel instead of one, plus
the SDF/normal math) — this is exactly what the GPU track's Section B.3
frame-time measurement is for. Expect this to only be affordable on 1-2
surfaces, which is consistent with Apple's own selective usage and the
design rule in Section 1.

---

## 4. Design tokens

| Token | Value | Notes |
|---|---|---|
| Corner radius (standard) | 20px | matches the glassmorphism reference |
| Corner radius (liquid glass surfaces) | 28-37px | rounder, matches the React reference's 37px |
| Glass tint (light content behind) | `rgba(255,255,255,0.14)` | from the CSS reference, tune per-surface |
| Border | 1px, `rgba(255,255,255,0.3)`, brighter at top edge | |
| Blur radius (glassmorphism) | 20px equivalent | tune against real frame-time measurement, not fixed blindly |
| Blur radius (liquid glass base layer) | lighter than glassmorphism — the refraction does most of the visual work | |
| Chromatic aberration offset | 1-3px equivalent, edge-band only | too much looks broken, not glass |

## 5. Motion / animation

Apple's principle: Liquid Glass elements respond to movement (cursor/focus/
device tilt on real devices). For a desktop compositor without device tilt,
the equivalent input is cursor position and focus state:
- **On focus/hover:** specular highlight direction shifts slightly toward
  the cursor position (a cheap uniform update, not a re-render of the
  whole effect).
- **On window open/close:** scale + fade, not just fade — glass surfaces
  should feel like they have a small amount of "give" (a slight overshoot/
  settle on open, matching the "fluid dynamics" principle) rather than a
  linear fade. A simple spring/ease-out-back curve is enough; this doesn't
  need physics simulation.
- **On drag/move:** the backdrop sample should update live (it already
  will, since it's sampling the real framebuffer each frame) — no
  additional work needed beyond making sure the blur/refraction passes
  run every frame the surface moves, not just on first render.

---

## 6. What GLM should build, in order

1. Glassmorphism (Section 2) first — cheaper, lower risk, covers most
   surfaces, directly reuses the blur pass already scoped in the GPU
   track's Section B.
2. Apply it to the highest-visibility surface per the GPU track's Section
   D (taskbar or Kairos sidebar).
3. Only after that's proven and frame-time-measured: attempt Liquid Glass
   (Section 3) on ONE surface — the Kairos sidebar is the natural choice,
   both because it's the "always alive" surface Apple's own design intent
   targets this effect at, and because it's already the highest-visibility
   candidate from the GPU track.
4. Measure real frame-time cost before deciding whether Liquid Glass
   extends to a second surface (the launcher/dock) or stays a single
   signature moment.

## 7. References
- Apple, "Meet Liquid Glass" / "Get to know the new design system" (WWDC
  2025 sessions 219/356) — design principles (reflective, fluid, depth)
- `OverShifted/LiquidGlass` (GitHub, MIT) — closest real native (OpenGL/
  GLSL, not web) reference implementation; confirms the SDF+refraction+
  blur+noise+glow technique set
- Aghajari, "Liquid Glass: iOS Effect Explanation" (Medium) — five-
  component technical breakdown (GPU shaders, SDF shape, lens-distortion
  refraction, edge-only chromatic aberration, blur) used as the structure
  for Section 3 above
- User-supplied CSS glassmorphism reference and SVG-filter React
  component — source material for Sections 2 and 3's web-to-GLSL
  translation; both tools (hype4.academy generator, user's own liquid-
  glass customizer) remain useful for iterating on visual parameters
  (tint, radius, blur amount) before committing values into the GLSL
  uniforms
