/**
 * @file config.c
 * @brief INI config parsing for WLT-Terminal
 * @module wlt-terminal/config
 * @purpose Load font, colors, scrollback, cursor, bell settings
 * @dependencies: terminal.h
 * @tested_by: tests/userspace/config_test.c
 * @spec: docs/specs/wlt-terminal.md
 */

#include "terminal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ─── Constants ──────────────────────────────────────── */

#define WLT_CONFIG_MAX_LINE 512
#define WLT_CONFIG_MAX_KEY 64
#define WLT_CONFIG_MAX_VAL 256
#define WLT_CONFIG_DEFAULT_PATH "/etc/wlt/terminal.conf"

/* ─── Defaults ───────────────────────────────────────── */

static const wlt_Color DEFAULT_PALETTE[WLT_TERMINAL_NUM_COLORS] = {
    {0x15, 0x16, 0x1e}, {0xf7, 0x76, 0x8e},
    {0x9e, 0xce, 0x6a}, {0xe0, 0xaf, 0x68},
    {0x7a, 0xa2, 0xf7}, {0xbb, 0x9a, 0xf7},
    {0x7d, 0xcf, 0xff}, {0xa9, 0xb1, 0xd6},
    {0x41, 0x48, 0x68}, {0xf7, 0x76, 0x8e},
    {0x9e, 0xce, 0x6a}, {0xe0, 0xaf, 0x68},
    {0x7a, 0xa2, 0xf7}, {0xbb, 0x9a, 0xf7},
    {0x7d, 0xcf, 0xff}, {0xc0, 0xca, 0xf5},
};

void
wlt_config_default(wlt_Config *out)
{
    if (out == NULL) return;
    memset(out, 0, sizeof(*out));

    strncpy(out->font, "monospace", sizeof(out->font) - 1);
    out->font_size = 14;
    out->scheme = WLT_COLOR_SCHEME_DARK;
    out->scrollback_lines = WLT_TERMINAL_MAX_SCROLLBACK;
    out->cursor_style = WLT_CURSOR_BLOCK;
    out->bell = WLT_BELL_NONE;
    out->background_opacity = 1.0f;
    strncpy(out->shell, "/bin/sh", sizeof(out->shell) - 1);

    memcpy(out->palette, DEFAULT_PALETTE, sizeof(DEFAULT_PALETTE));
    out->default_fg = (wlt_Color){0xc0, 0xca, 0xf5};
    out->default_bg = (wlt_Color){0x1a, 0x1b, 0x26};
    out->cursor_color = (wlt_Color){0xc0, 0xca, 0xf5};
    out->selection_color = (wlt_Color){0x33, 0x46, 0x7c};
}

void
wlt_config_destroy(wlt_Config *cfg)
{
    /* Nothing heap-allocated; here for symmetry with create/destroy API */
    (void)cfg;
}

/* ─── INI Parsing Helpers ────────────────────────────── */

static char *
trim(char *s)
{
    /* Skip leading whitespace */
    char *start = s;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);

    /* Trim trailing whitespace in-place */
    if (*s)
    {
        char *end = s + strlen(s) - 1;
        while (end > s && isspace((unsigned char)*end))
            *end-- = '\0';
    }
    return s;
}

static int
split_kv(const char *line, char *key, size_t key_max,
         char *val, size_t val_max)
{
    const char *eq = strchr(line, '=');
    if (eq == NULL) return -1;

    size_t klen = (size_t)(eq - line);
    if (klen >= key_max) klen = key_max - 1;
    memcpy(key, line, klen);
    key[klen] = '\0';
    trim(key);

    const char *vs = eq + 1;
    size_t vlen = strlen(vs);
    if (vlen >= val_max) vlen = val_max - 1;
    memcpy(val, vs, vlen);
    val[vlen] = '\0';
    trim(val);
    return 0;
}

/**
 * Parse "#RRGGBB" into a wlt_Color. Returns 0 on success.
 */
static int
parse_color(const char *s, wlt_Color *out)
{
    if (s == NULL || out == NULL) return -1;
    if (s[0] == '#') s++;
    if (strlen(s) != 6) return -1;
    char rs[3] = {s[0], s[1], 0};
    char gs[3] = {s[2], s[3], 0};
    char bs[3] = {s[4], s[5], 0};
    out->r = (uint8_t)strtoul(rs, NULL, 16);
    out->g = (uint8_t)strtoul(gs, NULL, 16);
    out->b = (uint8_t)strtoul(bs, NULL, 16);
    return 0;
}

static bool
parse_bool(const char *s)
{
    return (strcmp(s, "yes") == 0 ||
            strcmp(s, "true") == 0 ||
            strcmp(s, "1") == 0);
}

static void
set_palette_entry(wlt_Config *cfg, const char *key, const char *val)
{
    /* key like "color0" .. "color15" */
    if (strncmp(key, "color", 5) != 0) return;
    int idx = atoi(key + 5);
    if (idx < 0 || idx >= WLT_TERMINAL_NUM_COLORS) return;
    wlt_Color c;
    if (parse_color(val, &c) == 0)
        cfg->palette[idx] = c;
}

/* ─── Line Processing ────────────────────────────────── */

static void
apply_kv(wlt_Config *cfg, const char *section,
         const char *key, const char *val)
{
    if (strcmp(section, "wlt-terminal") == 0)
    {
        if (strcmp(key, "font") == 0)
        {
            strncpy(cfg->font, val, sizeof(cfg->font) - 1);
            cfg->font[sizeof(cfg->font) - 1] = '\0';
        }
        else if (strcmp(key, "font_size") == 0)
        {
            cfg->font_size = (uint32_t)strtoul(val, NULL, 10);
        }
        else if (strcmp(key, "color_scheme") == 0)
        {
            if (strcmp(val, "dark") == 0)
                cfg->scheme = WLT_COLOR_SCHEME_DARK;
            else if (strcmp(val, "light") == 0)
                cfg->scheme = WLT_COLOR_SCHEME_LIGHT;
            else
                cfg->scheme = WLT_COLOR_SCHEME_CUSTOM;
        }
        else if (strcmp(key, "scrollback_lines") == 0)
        {
            uint32_t n = (uint32_t)strtoul(val, NULL, 10);
            if (n > 0 && n <= WLT_TERMINAL_MAX_SCROLLBACK)
                cfg->scrollback_lines = n;
        }
        else if (strcmp(key, "cursor_style") == 0)
        {
            if (strcmp(val, "underline") == 0)
                cfg->cursor_style = WLT_CURSOR_UNDERLINE;
            else if (strcmp(val, "bar") == 0)
                cfg->cursor_style = WLT_CURSOR_BAR;
            else
                cfg->cursor_style = WLT_CURSOR_BLOCK;
        }
        else if (strcmp(key, "bell") == 0)
        {
            if (strcmp(val, "sound") == 0)
                cfg->bell = WLT_BELL_SOUND;
            else if (strcmp(val, "visual") == 0)
                cfg->bell = WLT_BELL_VISUAL;
            else
                cfg->bell = WLT_BELL_NONE;
        }
        else if (strcmp(key, "background_opacity") == 0)
        {
            float f = strtof(val, NULL);
            if (f < 0.0f) f = 0.0f;
            if (f > 1.0f) f = 1.0f;
            cfg->background_opacity = f;
        }
        else if (strcmp(key, "shell") == 0)
        {
            strncpy(cfg->shell, val, sizeof(cfg->shell) - 1);
            cfg->shell[sizeof(cfg->shell) - 1] = '\0';
        }
    }
    else if (strcmp(section, "colors") == 0)
    {
        if (strcmp(key, "background") == 0)
            parse_color(val, &cfg->default_bg);
        else if (strcmp(key, "foreground") == 0)
            parse_color(val, &cfg->default_fg);
        else if (strcmp(key, "cursor") == 0)
            parse_color(val, &cfg->cursor_color);
        else if (strcmp(key, "selection") == 0)
            parse_color(val, &cfg->selection_color);
        else if (strncmp(key, "color", 5) == 0)
            set_palette_entry(cfg, key, val);
    }
    (void)parse_bool;
}

/* ─── Public API ─────────────────────────────────────── */

wlt_terminal_error_t
wlt_config_load(const char *path, wlt_Config *out)
{
    if (out == NULL)
        return WLT_TERMINAL_ERR_INVALID_ARG;
    if (path == NULL || path[0] == '\0')
        path = WLT_CONFIG_DEFAULT_PATH;

    /* Always start from defaults so missing keys keep sane values */
    wlt_config_default(out);

    FILE *f = fopen(path, "r");
    if (f == NULL)
        return WLT_TERMINAL_ERR_NOT_FOUND;

    char section[WLT_CONFIG_MAX_KEY] = "wlt-terminal";
    char line[WLT_CONFIG_MAX_LINE];

    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *s = trim(line);
        if (*s == '\0' || *s == '#' || *s == ';')
            continue;

        if (*s == '[')
        {
            char *end = strchr(s, ']');
            if (end != NULL)
            {
                *end = '\0';
                strncpy(section, s + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
                trim(section);
            }
            continue;
        }

        char key[WLT_CONFIG_MAX_KEY];
        char val[WLT_CONFIG_MAX_VAL];
        if (split_kv(s, key, sizeof(key), val, sizeof(val)) == 0)
            apply_kv(out, section, key, val);
    }

    fclose(f);
    return WLT_TERMINAL_OK;
}
