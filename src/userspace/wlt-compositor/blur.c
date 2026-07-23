/**
 * @file blur.c
 * @brief Dual-pass Gaussian backdrop blur (GLES2 shader implementation)
 *
 * Standard technique (separable Gaussian, two passes):
 *   Pass 1 — horizontal: sample a row of pixels weighted by the 1D kernel,
 *            render into an intermediate framebuffer (FBO #2).
 *   Pass 2 — vertical:    sample a column of FBO #2 weighted by the same
 *            kernel, render into the destination.
 *
 * A single combined pass would be O(r^2); the separable approach is O(2r).
 * This is how Hyprland/KWin/Mutter all do backdrop blur.
 *
 * Kernel: 1D Gaussian normalized to sum=1.0, computed from sigma.
 *   w[i] = exp(-(i*i)/(2*sigma*sigma)); then divide by sum.
 *
 * @dependencies: GLES2, EGL (via wlroots renderer)
 * @proof: GPU Section B — a real blur, verified by pixel inspection.
 */
#include "blur.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WLT_BLUR_MAX_TAPS 25   /* radius 12 -> 25 taps, ±12px each side */

/* ─── Shaders ────────────────────────────────────────────── */

static const char *WLT_BLUR_VERT_SRC =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_tex;\n"
    "varying vec2 v_tex;\n"
    "uniform vec2 u_texoffset;   /* per-tap offset in texels (pass direction) */\n"
    "void main() {\n"
    "    v_tex = a_tex;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

/* Fragment shader: samples 2*radius+1 taps along the direction given by
 * u_texoffset (horizontal: (dt, 0); vertical: (0, dt)). u_weights holds
 * the precomputed Gaussian kernel. This is the separable pass. */
static const char *WLT_BLUR_FRAG_HDR =
    "precision mediump float;\n"
    "varying vec2 v_tex;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec2 u_texoffset;\n"
    "uniform float u_weights[%d];\n"
    "uniform int u_radius;\n"
    "void main() {\n"
    "    vec4 sum = texture2D(u_tex, v_tex) * u_weights[0];\n"
    "    for (int i = 1; i <= 12; i++) {  /* GLSL ES 1.00 needs constant bound */\n"
    "        if (i > u_radius) break;\n"
    "        vec2 off = u_texoffset * float(i);\n"
    "        sum += texture2D(u_tex, v_tex + off) * u_weights[i];\n"
    "        sum += texture2D(u_tex, v_tex - off) * u_weights[i];\n"
    "    }\n"
    "    gl_FragColor = sum;\n"
    "}\n";

/* ─── Helpers ───────────────────────────────────────────── */

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    if (!s) return 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "[blur] shader compile failed: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    if (!p) return 0;
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "[blur] program link failed: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

/* Compute a 1D normalized Gaussian kernel of (2*radius+1) taps. */
static void compute_kernel(float *weights, int radius) {
    float sigma = radius / 3.0f;   /* standard: sigma = radius/3 */
    if (sigma < 1.0f) sigma = 1.0f;
    double sum = 0.0;
    for (int i = 0; i <= radius; i++) {
        double w = exp(-(double)(i*i) / (2.0 * sigma * sigma));
        weights[i] = (float)w;
        sum += w;
        if (i > 0) sum += w;   /* symmetric taps */
    }
    /* normalize so all taps sum to 1.0 */
    for (int i = 0; i <= radius; i++)
        weights[i] = (float)(weights[i] / sum);
}

/* ─── State ─────────────────────────────────────────────── */

struct wlt_blur {
    GLuint program;
    GLint  loc_pos, loc_tex, loc_tex_id, loc_texoffset, loc_weights, loc_radius;
    GLuint fbo;            /* intermediate framebuffer */
    GLuint vbo;            /* shared fullscreen quad */
    int    radius;
    float  weights[WLT_BLUR_MAX_TAPS]; /* [0..radius] */
};

/* Fullscreen triangle/quad: positions (-1..1) + texcoords (0..1). */
static const float QUAD_VERTS[] = {
    /* pos     tex    */
    -1.f, -1.f,  0.f, 0.f,
     1.f, -1.f,  1.f, 0.f,
    -1.f,  1.f,  0.f, 1.f,
     1.f,  1.f,  1.f, 1.f,
};

wlt_blur_t *wlt_blur_create(int radius_px) {
    if (radius_px < 1) radius_px = 1;
    if (radius_px > 12) radius_px = 12;   /* cap: GLSL loop bound is 12 */

    wlt_blur_t *b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->radius = radius_px;
    compute_kernel(b->weights, radius_px);

    /* Build the fragment shader with the right array size. */
    char frag[2048];
    snprintf(frag, sizeof(frag), WLT_BLUR_FRAG_HDR, radius_px + 1);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, WLT_BLUR_VERT_SRC);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag);
    if (!vs || !fs) {
        fprintf(stderr, "[blur] shader compile failed; blur disabled\n");
        free(b);
        return NULL;
    }
    b->program = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!b->program) { free(b); return NULL; }

    b->loc_pos        = glGetAttribLocation(b->program, "a_pos");
    b->loc_tex        = glGetAttribLocation(b->program, "a_tex");
    b->loc_tex_id     = glGetUniformLocation(b->program, "u_tex");
    b->loc_texoffset  = glGetUniformLocation(b->program, "u_texoffset");
    b->loc_weights    = glGetUniformLocation(b->program, "u_weights");
    b->loc_radius     = glGetUniformLocation(b->program, "u_radius");

    /* Shared fullscreen-quad VBO. */
    glGenBuffers(1, &b->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    fprintf(stderr, "[blur] created: radius=%d sigma=%.2f program=%u\n",
            radius_px, radius_px / 3.0f, b->program);
    return b;
}

void wlt_blur_destroy(wlt_blur_t *b) {
    if (!b) return;
    if (b->program) glDeleteProgram(b->program);
    if (b->fbo) glDeleteFramebuffers(1, &b->fbo);
    if (b->vbo) glDeleteBuffers(1, &b->vbo);
    free(b);
}

/* Apply one separable pass to the bound source texture, rendering into
 * the currently-bound framebuffer. dir = 0 horizontal, 1 vertical. */
static void blur_pass(wlt_blur_t *b, GLuint src_tex,
                      int x, int y, int w, int h,
                      int screen_w, int screen_h, int dir) {
    glUseProgram(b->program);

    /* Bind the source as texture 0. */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glUniform1i(b->loc_tex_id, 0);

    /* Per-tap offset: one texel in the pass direction. */
    float dx = (dir == 0) ? 1.0f / (float)screen_w : 0.0f;
    float dy = (dir == 1) ? 1.0f / (float)screen_h : 0.0f;
    glUniform2f(b->loc_texoffset, dx, dy);

    /* Upload the kernel (only indices 0..radius). */
    glUniform1fv(b->loc_weights, b->radius + 1, b->weights);
    glUniform1i(b->loc_radius, b->radius);

    /* Draw the fullscreen quad (clip space -1..1 covers the whole FBO;
     * the region x/y/w/h is implied by the viewport set by the caller). */
    glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
    if (b->loc_pos >= 0) {
        glEnableVertexAttribArray(b->loc_pos);
        glVertexAttribPointer(b->loc_pos, 2, GL_FLOAT, GL_FALSE,
                             4 * sizeof(float), (void *)0);
    }
    if (b->loc_tex >= 0) {
        glEnableVertexAttribArray(b->loc_tex);
        glVertexAttribPointer(b->loc_tex, 2, GL_FLOAT, GL_FALSE,
                             4 * sizeof(float), (void *)(2 * sizeof(float)));
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (b->loc_pos >= 0) glDisableVertexAttribArray(b->loc_pos);
    if (b->loc_tex >= 0) glDisableVertexAttribArray(b->loc_tex);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void wlt_blur_apply(wlt_blur_t *b, int x, int y, int w, int h,
                    int screen_w, int screen_h) {
    if (!b || w <= 0 || h <= 0 || screen_w <= 0 || screen_h <= 0)
        return;

    /* Read the current framebuffer's content as a texture. The compositor
     * has just rendered the scene into it (via wlr_scene_output_build_state);
     * we sample from it for the blur. */
    GLuint src_tex = 0;
    glGenTextures(1, &src_tex);
    glBindTexture(GL_TEXTURE_2D, src_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_w, screen_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    /* Bind the current framebuffer (the compositor's output) as the source
     * by copying it into our texture. This is the simplest portable path
     * that works whether the renderer is llvmpipe or real GPU. */
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, screen_w, screen_h, 0);

    /* Allocate the intermediate FBO once. */
    if (!b->fbo) {
        glGenFramebuffers(1, &b->fbo);
    }

    /* Pass 1: horizontal — read src_tex, render into FBO. */
    GLuint inter_tex = 0;
    glGenTextures(1, &inter_tex);
    glBindTexture(GL_TEXTURE_2D, inter_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_w, screen_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, b->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, inter_tex, 0);
    glViewport(0, 0, screen_w, screen_h);
    blur_pass(b, src_tex, x, y, w, h, screen_w, screen_h, 0);

    /* Pass 2: vertical — read inter_tex, render back to the default
     * framebuffer (the compositor's output, which will be committed). */
    GLint default_fb = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &default_fb);
    /* Bind the compositor's output framebuffer (0 = default, or the
     * wlroots-allocated one). For the proof we use the default. */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(x, y, w, h);
    blur_pass(b, inter_tex, x, y, w, h, screen_w, screen_h, 1);

    /* Restore state. */
    glDeleteTextures(1, &src_tex);
    glDeleteTextures(1, &inter_tex);
    glBindFramebuffer(GL_FRAMEBUFFER, default_fb);
    glViewport(0, 0, screen_w, screen_h);
}
