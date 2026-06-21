/**
 * @file terminal.h
 * @brief Public interface for WLT-Terminal emulator
 * @module wlt-terminal
 * @purpose Emulates a VT100/ANSI terminal: cell grid, cursor, input routing
 * @dependencies: pty.h, ansi.h, scrollback, config, render
 * @tested_by: tests/userspace/terminal_test.c
 * @spec: docs/specs/wlt-terminal.md
 * @adr: docs/adr/05-wayland-wlroots.md
 */

#ifndef WLT_TERMINAL_H
#define WLT_TERMINAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error Codes ────────────────────────────────────── */

typedef enum {
    WLT_TERMINAL_OK = 0,
    WLT_TERMINAL_ERR_INVALID_ARG = -1,
    WLT_TERMINAL_ERR_NOMEM = -2,
    WLT_TERMINAL_ERR_IO = -3,
    WLT_TERMINAL_ERR_PTY = -4,
    WLT_TERMINAL_ERR_NOT_FOUND = -5,
    WLT_TERMINAL_ERR_PARSE = -6,
    WLT_TERMINAL_ERR_TOO_LARGE = -7,
} wlt_terminal_error_t;

/* ─── Constants ──────────────────────────────────────── */

#define WLT_TERMINAL_VERSION "1.0.0"
#define WLT_TERMINAL_DEFAULT_COLS 80
#define WLT_TERMINAL_DEFAULT_ROWS 24
#define WLT_TERMINAL_MAX_COLS 500
#define WLT_TERMINAL_MAX_ROWS 200
#define WLT_TERMINAL_MAX_SCROLLBACK 10000
#define WLT_TERMINAL_TAB_WIDTH 8
#define WLT_TERMINAL_NUM_COLORS 16
#define WLT_TERMINAL_MAX_PATH 256
#define WLT_TERMINAL_MAX_FONT_NAME 128

/* ─── Forward Declarations ───────────────────────────── */

typedef struct wlt_Pty wlt_Pty;
typedef struct wlt_AnsiParser wlt_AnsiParser;
typedef struct wlt_Scrollback wlt_Scrollback;
typedef struct wlt_Config wlt_Config;
typedef struct wlt_Framebuffer wlt_Framebuffer;
typedef struct wlt_Terminal wlt_Terminal;

/* ─── Color Types ────────────────────────────────────── */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} wlt_Color;

typedef enum {
    WLT_COLOR_SCHEME_DARK = 0,
    WLT_COLOR_SCHEME_LIGHT,
    WLT_COLOR_SCHEME_CUSTOM,
} wlt_color_scheme_t;

typedef enum {
    WLT_CURSOR_BLOCK = 0,
    WLT_CURSOR_UNDERLINE,
    WLT_CURSOR_BAR,
} wlt_cursor_style_t;

typedef enum {
    WLT_BELL_NONE = 0,
    WLT_BELL_SOUND,
    WLT_BELL_VISUAL,
} wlt_bell_mode_t;

/* ─── Cell & Cursor ──────────────────────────────────── */

/**
 * A single character cell on the terminal grid.
 * Holds a Unicode codepoint plus rendering attributes.
 */
typedef struct {
    uint32_t codepoint;     /* Unicode codepoint, 0 = empty */
    uint8_t fg;             /* Foreground palette index (0-15) */
    uint8_t bg;             /* Background palette index (0-15) */
    bool bold;
    bool italic;
    bool underline;
    bool reverse;
    bool blink;
    bool dirty;             /* Changed since last render */
} wlt_Cell;

typedef struct {
    uint32_t row;
    uint32_t col;
    bool visible;
} wlt_Cursor;

/**
 * Active text attributes during output parsing.
 * Reset by SGR 0.
 */
typedef struct {
    uint8_t fg;
    uint8_t bg;
    bool bold;
    bool italic;
    bool underline;
    bool reverse;
    bool blink;
} wlt_CellAttr;

/* ─── Terminal State ─────────────────────────────────── */

/**
 * Main terminal state. Owns the visible cell grid, cursor,
 * PTY, ANSI parser, scrollback buffer, and current palette.
 */
struct wlt_Terminal {
    uint32_t cols;
    uint32_t rows;
    wlt_Cell *grid;             /* cols * rows cells */
    wlt_Cursor cursor;
    wlt_Cursor saved_cursor;    /* For DECSC/DECRC */
    wlt_CellAttr current_attr;  /* Attributes applied to new cells */

    /* Auto-wrap (DECAWM) handling */
    bool wrap_pending;          /* Last char hit right margin */

    /* Scroll region (DECSTBM) */
    uint32_t scroll_top;        /* Inclusive, row index */
    uint32_t scroll_bottom;     /* Exclusive, row index */

    /* Modes */
    bool insert_mode;           /* IRM */
    bool auto_wrap;             /* DECAWM */
    bool cursor_visible;
    bool application_keypad;
    bool application_cursor;

    /* Palette + scheme colors */
    wlt_Color palette[WLT_TERMINAL_NUM_COLORS];
    wlt_Color default_fg;
    wlt_Color default_bg;
    wlt_Color cursor_color;
    wlt_Color selection_color;

    /* Subsystems */
    wlt_Pty *pty;
    wlt_AnsiParser *parser;
    wlt_Scrollback *scrollback;
    wlt_Config *config;

    /* Scrollback viewport offset (lines above bottom) */
    uint32_t scroll_offset;

    /* Selection */
    bool has_selection;
    uint32_t sel_start_row;
    uint32_t sel_start_col;
    uint32_t sel_end_row;
    uint32_t sel_end_col;

    /* Bell + redraw flags */
    bool bell_pending;
    bool full_redraw;

    /* Cursor style */
    wlt_cursor_style_t cursor_style;
    wlt_bell_mode_t bell_mode;
};

/* ─── Public Terminal API ────────────────────────────── */

/**
 * Create a new terminal with the given dimensions.
 * Spawns the shell on a PTY. Caller frees via destroy.
 */
wlt_terminal_error_t
wlt_terminal_create(wlt_Terminal **out,
                    uint32_t cols,
                    uint32_t rows,
                    const wlt_Config *config);

/**
 * Destroy a terminal and free all owned resources.
 * Closes the PTY and reaps the shell.
 */
void
wlt_terminal_destroy(wlt_Terminal *term);

/**
 * Feed shell output bytes into the terminal.
 * Parses ANSI sequences and updates the grid.
 */
wlt_terminal_error_t
wlt_terminal_output(wlt_Terminal *term,
                    const uint8_t *data,
                    size_t len);

/**
 * Send user input (key presses) to the shell via the PTY.
 */
wlt_terminal_error_t
wlt_terminal_input(wlt_Terminal *term,
                   const uint8_t *data,
                   size_t len);

/**
 * Resize the terminal grid. Existing content is preserved
 * where possible; scrolled lines move into scrollback.
 */
wlt_terminal_error_t
wlt_terminal_resize(wlt_Terminal *term,
                    uint32_t new_cols,
                    uint32_t new_rows);

/**
 * Scroll the viewport up (negative) or down (positive) by lines.
 */
wlt_terminal_error_t
wlt_terminal_scroll_view(wlt_Terminal *term,
                         int32_t lines);

/**
 * Reset scrollback offset to the bottom (most recent output).
 */
void
wlt_terminal_scroll_to_bottom(wlt_Terminal *term);

/**
 * Render the visible grid + scrollback into a framebuffer.
 */
wlt_terminal_error_t
wlt_terminal_render(wlt_Terminal *term,
                    wlt_Framebuffer *fb);

/**
 * Pump pending PTY output through the parser and into the grid.
 * Non-blocking; returns WLT_TERMINAL_OK if no data was available.
 */
wlt_terminal_error_t
wlt_terminal_pump(wlt_Terminal *term);

/**
 * Mark every cell dirty, forcing a full redraw next render.
 */
void
wlt_terminal_invalidate(wlt_Terminal *term);

/**
 * Get current cursor position.
 */
void
wlt_terminal_cursor_pos(const wlt_Terminal *term,
                        uint32_t *row,
                        uint32_t *col);

const char *
wlt_terminal_error_str(wlt_terminal_error_t err);

/* ─── Scrollback API (module-internal) ───────────────── */

wlt_terminal_error_t
wlt_scrollback_create(wlt_Scrollback **out,
                      uint32_t capacity,
                      uint32_t cols);

void
wlt_scrollback_destroy(wlt_Scrollback *sb);

void
wlt_scrollback_push_row(wlt_Scrollback *sb,
                        const wlt_Cell *cells,
                        uint32_t cols);

const wlt_Cell *
wlt_scrollback_peek(wlt_Scrollback *sb, uint32_t from_end);

uint32_t
wlt_scrollback_size(wlt_Scrollback *sb);

/* ─── Config API ─────────────────────────────────────── */

struct wlt_Config {
    char font[WLT_TERMINAL_MAX_FONT_NAME];
    uint32_t font_size;
    wlt_color_scheme_t scheme;
    uint32_t scrollback_lines;
    wlt_cursor_style_t cursor_style;
    wlt_bell_mode_t bell;
    float background_opacity;
    char shell[WLT_TERMINAL_MAX_PATH];
    wlt_Color palette[WLT_TERMINAL_NUM_COLORS];
    wlt_Color default_fg;
    wlt_Color default_bg;
    wlt_Color cursor_color;
    wlt_Color selection_color;
};

wlt_terminal_error_t
wlt_config_load(const char *path, wlt_Config *out);

void
wlt_config_default(wlt_Config *out);

void
wlt_config_destroy(wlt_Config *cfg);

/* ─── Render API ─────────────────────────────────────── */

/**
 * Pixel buffer used for rendering. Layout is 32-bit BGRA,
 * row-major with `stride` bytes per row.
 */
struct wlt_Framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint8_t bytes_per_pixel;
    uint8_t *data;
};

void
wlt_render_init_palette(wlt_Terminal *term, const wlt_Config *config);

uint8_t
wlt_render_nearest_palette(const wlt_Terminal *term,
                           uint8_t r, uint8_t g, uint8_t b);

wlt_terminal_error_t
wlt_render_draw(wlt_Terminal *term, wlt_Framebuffer *fb);

#ifdef __cplusplus
}
#endif

#endif /* WLT_TERMINAL_H */
