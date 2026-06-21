/**
 * @file terminal.c
 * @brief Main terminal emulator logic — cell grid, cursor, ANSI dispatch
 * @module wlt-terminal/core
 * @purpose Wires PTY output through the ANSI parser into a cell grid
 * @dependencies: terminal.h, pty.h, ansi.h, scrollback, config, render
 * @tested_by: tests/userspace/terminal_test.c
 * @spec: docs/specs/wlt-terminal.md
 */

#include "terminal.h"
#include "pty.h"
#include "ansi.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── Helpers ────────────────────────────────────────── */

static wlt_CellAttr
default_attr(void)
{
    wlt_CellAttr a;
    a.fg = 7;       /* Default to light gray */
    a.bg = 0;
    a.bold = false;
    a.italic = false;
    a.underline = false;
    a.reverse = false;
    a.blink = false;
    return a;
}

static inline wlt_Cell *
cell_at(wlt_Terminal *term, uint32_t row, uint32_t col)
{
    return &term->grid[row * term->cols + col];
}

static void
clear_cell(wlt_Cell *cell, const wlt_CellAttr *attr)
{
    cell->codepoint = ' ';
    cell->fg = attr->fg;
    cell->bg = attr->bg;
    cell->bold = attr->bold;
    cell->italic = attr->italic;
    cell->underline = attr->underline;
    cell->reverse = attr->reverse;
    cell->blink = attr->blink;
    cell->dirty = true;
}

static void
clear_row(wlt_Terminal *term, uint32_t row)
{
    for (uint32_t c = 0; c < term->cols; c++)
        clear_cell(cell_at(term, row, c), &term->current_attr);
}

static void
clear_region(wlt_Terminal *term,
             uint32_t r0, uint32_t c0,
             uint32_t r1, uint32_t c1)
{
    for (uint32_t r = r0; r <= r1; r++)
    {
        uint32_t end_col = (r == r1) ? c1 : term->cols - 1;
        uint32_t start_col = (r == r0) ? c0 : 0;
        for (uint32_t c = start_col; c <= end_col; c++)
            clear_cell(cell_at(term, r, c), &term->current_attr);
    }
}

static void
clamp_cursor(const wlt_Terminal *term, wlt_Cursor *c)
{
    if (c->row >= term->rows) c->row = term->rows - 1;
    if (c->col >= term->cols) c->col = term->cols - 1;
}

/* ─── Grid Scrolling ─────────────────────────────────── */

static void
scroll_up(wlt_Terminal *term, uint32_t n)
{
    uint32_t region = term->scroll_bottom - term->scroll_top;
    if (n > region) n = region;
    if (n == 0) return;

    /* Push scrolled-out lines into scrollback */
    for (uint32_t i = 0; i < n; i++)
        wlt_scrollback_push_row(term->scrollback,
                                &term->grid[(term->scroll_top + i)
                                            * term->cols],
                                term->cols);

    uint32_t span = region - n;
    if (span > 0)
        memmove(&term->grid[term->scroll_top * term->cols],
                &term->grid[(term->scroll_top + n) * term->cols],
                span * term->cols * sizeof(wlt_Cell));
    for (uint32_t i = 0; i < n; i++)
        clear_row(term, term->scroll_bottom - 1 - i);
}

static void
scroll_down(wlt_Terminal *term, uint32_t n)
{
    uint32_t region = term->scroll_bottom - term->scroll_top;
    if (n > region) n = region;
    if (n == 0) return;

    uint32_t span = region - n;
    if (span > 0)
        memmove(&term->grid[(term->scroll_top + n) * term->cols],
                &term->grid[term->scroll_top * term->cols],
                span * term->cols * sizeof(wlt_Cell));
    for (uint32_t i = 0; i < n; i++)
        clear_row(term, term->scroll_top + i);
}

static void
cursor_down(wlt_Terminal *term, uint32_t n)
{
    term->cursor.row += n;
    if (term->cursor.row >= term->scroll_bottom)
        term->cursor.row = term->scroll_bottom - 1;
}

static void
newline(wlt_Terminal *term)
{
    term->cursor.col = 0;
    term->wrap_pending = false;
    if (term->cursor.row + 1 >= term->scroll_bottom)
        scroll_up(term, 1);
    else
        term->cursor.row++;
}

static void
tab(wlt_Terminal *term)
{
    uint32_t next = (term->cursor.col / WLT_TERMINAL_TAB_WIDTH + 1)
                    * WLT_TERMINAL_TAB_WIDTH;
    if (next >= term->cols) next = term->cols - 1;
    term->cursor.col = next;
    term->wrap_pending = false;
}

/* ─── Print Path ─────────────────────────────────────── */

static void
on_print(void *ctx, uint32_t codepoint)
{
    wlt_Terminal *term = (wlt_Terminal *)ctx;

    if (term->wrap_pending)
    {
        newline(term);
        term->wrap_pending = false;
    }

    if (term->insert_mode)
    {
        wlt_Cell *row = &term->grid[term->cursor.row * term->cols];
        memmove(&row[term->cursor.col + 1],
                &row[term->cursor.col],
                (term->cols - term->cursor.col - 1) * sizeof(wlt_Cell));
    }

    wlt_Cell *cell = cell_at(term, term->cursor.row, term->cursor.col);
    cell->codepoint = codepoint;
    cell->fg = term->current_attr.fg;
    cell->bg = term->current_attr.bg;
    cell->bold = term->current_attr.bold;
    cell->italic = term->current_attr.italic;
    cell->underline = term->current_attr.underline;
    cell->reverse = term->current_attr.reverse;
    cell->blink = term->current_attr.blink;
    cell->dirty = true;

    if (term->cursor.col + 1 >= term->cols)
    {
        if (term->auto_wrap)
            term->wrap_pending = true;
    }
    else
    {
        term->cursor.col++;
    }
}

/* ─── C0 Control Execution ───────────────────────────── */

static void
on_execute(void *ctx, uint8_t control)
{
    wlt_Terminal *term = (wlt_Terminal *)ctx;
    switch (control)
    {
        case 0x07:  /* BEL */
            term->bell_pending = true;
            break;
        case 0x08:  /* BS — backspace */
            if (term->cursor.col > 0)
                term->cursor.col--;
            term->wrap_pending = false;
            break;
        case 0x09:  /* HT — tab */
            tab(term);
            break;
        case 0x0A:  /* LF */
        case 0x0B:
        case 0x0C:
            if (term->cursor.row + 1 >= term->scroll_bottom)
                scroll_up(term, 1);
            else
                term->cursor.row++;
            term->wrap_pending = false;
            break;
        case 0x0D:  /* CR */
            term->cursor.col = 0;
            term->wrap_pending = false;
            break;
        default:
            break;
    }
}

/* ─── CSI Dispatch ───────────────────────────────────── */

static uint32_t
param_or(const uint32_t *params, size_t n, size_t i, uint32_t def)
{
    if (i >= n || params[i] == 0) return def;
    return params[i];
}

static void
sgr_apply(wlt_Terminal *term, const uint32_t *params, size_t n)
{
    if (n == 0)
    {
        term->current_attr = default_attr();
        return;
    }
    for (size_t i = 0; i < n; i++)
    {
        uint32_t p = params[i];
        switch (p)
        {
            case 0: term->current_attr = default_attr(); break;
            case 1: term->current_attr.bold = true; break;
            case 3: term->current_attr.italic = true; break;
            case 4: term->current_attr.underline = true; break;
            case 5: term->current_attr.blink = true; break;
            case 7: term->current_attr.reverse = true; break;
            case 22: term->current_attr.bold = false; break;
            case 23: term->current_attr.italic = false; break;
            case 24: term->current_attr.underline = false; break;
            case 25: term->current_attr.blink = false; break;
            case 27: term->current_attr.reverse = false; break;
            case 38:
            case 48:
            {
                if (i + 1 >= n) break;
                uint32_t mode = params[i + 1];
                uint8_t *target = (p == 38)
                    ? &term->current_attr.fg
                    : &term->current_attr.bg;
                if (mode == 5 && i + 2 < n)
                {
                    *target = (uint8_t)(params[i + 2] & 0x0F);
                    i += 2;
                }
                else if (mode == 2 && i + 4 < n)
                {
                    *target = wlt_render_nearest_palette(
                        term,
                        (uint8_t)params[i + 2],
                        (uint8_t)params[i + 3],
                        (uint8_t)params[i + 4]);
                    i += 4;
                }
                break;
            }
            case 39: term->current_attr.fg = 7; break;
            case 49: term->current_attr.bg = 0; break;
            default:
                if (p >= 30 && p <= 37)
                    term->current_attr.fg = (uint8_t)(p - 30);
                else if (p >= 40 && p <= 47)
                    term->current_attr.bg = (uint8_t)(p - 40);
                else if (p >= 90 && p <= 97)
                    term->current_attr.fg = (uint8_t)(p - 90 + 8);
                else if (p >= 100 && p <= 107)
                    term->current_attr.bg = (uint8_t)(p - 100 + 8);
                break;
        }
    }
}

static void
on_csi(void *ctx, char final,
       const uint32_t *params, size_t nparams,
       const char *intermediate, size_t nint,
       bool private_marker)
{
    wlt_Terminal *term = (wlt_Terminal *)ctx;
    (void)intermediate; (void)nint;

    switch (final)
    {
        case 'A':  /* CUU — cursor up */
        {
            uint32_t n = param_or(params, nparams, 0, 1);
            if (term->cursor.row >= n)
                term->cursor.row -= n;
            else
                term->cursor.row = 0;
            term->wrap_pending = false;
            break;
        }
        case 'B':  /* CUD */
            cursor_down(term, param_or(params, nparams, 0, 1));
            break;
        case 'C':  /* CUF */
            term->cursor.col += param_or(params, nparams, 0, 1);
            if (term->cursor.col >= term->cols)
                term->cursor.col = term->cols - 1;
            term->wrap_pending = false;
            break;
        case 'D':  /* CUB */
        {
            uint32_t n = param_or(params, nparams, 0, 1);
            if (term->cursor.col >= n) term->cursor.col -= n;
            else term->cursor.col = 0;
            term->wrap_pending = false;
            break;
        }
        case 'E':  /* CNL */
            cursor_down(term, param_or(params, nparams, 0, 1));
            term->cursor.col = 0;
            break;
        case 'F':  /* CPL */
        {
            uint32_t n = param_or(params, nparams, 0, 1);
            if (term->cursor.row >= n) term->cursor.row -= n;
            else term->cursor.row = 0;
            term->cursor.col = 0;
            break;
        }
        case 'G':  /* CHA */
            term->cursor.col = param_or(params, nparams, 0, 1) - 1;
            clamp_cursor(term, &term->cursor);
            term->wrap_pending = false;
            break;
        case 'H':  /* CUP */
        case 'f':
            term->cursor.row = param_or(params, nparams, 0, 1) - 1;
            term->cursor.col = param_or(params, nparams, 1, 1) - 1;
            clamp_cursor(term, &term->cursor);
            term->wrap_pending = false;
            break;
        case 'J':  /* ED */
        {
            uint32_t mode = param_or(params, nparams, 0, 0);
            if (mode == 0)
                clear_region(term, term->cursor.row, term->cursor.col,
                             term->rows - 1, term->cols - 1);
            else if (mode == 1)
                clear_region(term, 0, 0,
                             term->cursor.row, term->cursor.col);
            else
                clear_region(term, 0, 0,
                             term->rows - 1, term->cols - 1);
            break;
        }
        case 'K':  /* EL */
        {
            uint32_t mode = param_or(params, nparams, 0, 0);
            uint32_t r = term->cursor.row;
            if (mode == 0)
                clear_region(term, r, term->cursor.col, r, term->cols - 1);
            else if (mode == 1)
                clear_region(term, r, 0, r, term->cursor.col);
            else
                clear_row(term, r);
            break;
        }
        case 'L':  /* IL — insert line */
        {
            uint32_t n = param_or(params, nparams, 0, 1);
            if (term->cursor.row < term->scroll_top ||
                term->cursor.row >= term->scroll_bottom)
                break;
            if (n > term->scroll_bottom - term->cursor.row)
                n = term->scroll_bottom - term->cursor.row;
            uint32_t span = term->scroll_bottom - term->cursor.row - n;
            if (span > 0)
                memmove(&term->grid[(term->cursor.row + n) * term->cols],
                        &term->grid[term->cursor.row * term->cols],
                        span * term->cols * sizeof(wlt_Cell));
            for (uint32_t i = 0; i < n; i++)
                clear_row(term, term->cursor.row + i);
            break;
        }
        case 'M':  /* DL — delete line */
        {
            uint32_t n = param_or(params, nparams, 0, 1);
            if (term->cursor.row < term->scroll_top ||
                term->cursor.row >= term->scroll_bottom)
                break;
            if (n > term->scroll_bottom - term->cursor.row)
                n = term->scroll_bottom - term->cursor.row;
            uint32_t span = term->scroll_bottom - term->cursor.row - n;
            if (span > 0)
                memmove(&term->grid[term->cursor.row * term->cols],
                        &term->grid[(term->cursor.row + n) * term->cols],
                        span * term->cols * sizeof(wlt_Cell));
            for (uint32_t i = 0; i < n; i++)
                clear_row(term, term->scroll_bottom - 1 - i);
            break;
        }
        case 'P':  /* DCH — delete character */
        {
            uint32_t n = param_or(params, nparams, 0, 1);
            wlt_Cell *row = &term->grid[term->cursor.row * term->cols];
            uint32_t remaining = term->cols - term->cursor.col;
            if (n > remaining) n = remaining;
            memmove(&row[term->cursor.col],
                    &row[term->cursor.col + n],
                    (remaining - n) * sizeof(wlt_Cell));
            for (uint32_t i = 0; i < n; i++)
                clear_cell(&row[term->cols - 1 - i], &term->current_attr);
            break;
        }
        case '@':  /* ICH — insert character */
        {
            uint32_t n = param_or(params, nparams, 0, 1);
            wlt_Cell *row = &term->grid[term->cursor.row * term->cols];
            uint32_t remaining = term->cols - term->cursor.col;
            if (n > remaining) n = remaining;
            memmove(&row[term->cursor.col + n],
                    &row[term->cursor.col],
                    (remaining - n) * sizeof(wlt_Cell));
            for (uint32_t i = 0; i < n; i++)
                clear_cell(&row[term->cursor.col + i],
                           &term->current_attr);
            break;
        }
        case 'S':  /* SU */
            scroll_up(term, param_or(params, nparams, 0, 1));
            break;
        case 'T':  /* SD */
            scroll_down(term, param_or(params, nparams, 0, 1));
            break;
        case 'm':  /* SGR */
            sgr_apply(term, params, nparams);
            break;
        case 'r':  /* DECSTBM — scroll region */
        {
            uint32_t top = param_or(params, nparams, 0, 1) - 1;
            uint32_t bot = param_or(params, nparams, 1, term->rows);
            if (top < bot && bot <= term->rows)
            {
                term->scroll_top = top;
                term->scroll_bottom = bot;
                term->cursor.row = 0;
                term->cursor.col = 0;
                term->wrap_pending = false;
            }
            break;
        }
        case 's':  /* save cursor */
            term->saved_cursor = term->cursor;
            break;
        case 'u':  /* restore cursor */
            term->cursor = term->saved_cursor;
            clamp_cursor(term, &term->cursor);
            break;
        case 'h':  /* set mode */
            if (private_marker)
            {
                for (size_t i = 0; i < nparams; i++)
                    if (params[i] == 25) term->cursor_visible = true;
            }
            else
            {
                for (size_t i = 0; i < nparams; i++)
                {
                    if (params[i] == 4) term->insert_mode = true;
                    if (params[i] == 20) term->auto_wrap = true;
                }
            }
            break;
        case 'l':  /* reset mode */
            if (private_marker)
            {
                for (size_t i = 0; i < nparams; i++)
                    if (params[i] == 25) term->cursor_visible = false;
            }
            else
            {
                for (size_t i = 0; i < nparams; i++)
                {
                    if (params[i] == 4) term->insert_mode = false;
                    if (params[i] == 20) term->auto_wrap = false;
                }
            }
            break;
        default:
            break;
    }
}

static void
on_esc(void *ctx, char final,
       const char *intermediate, size_t nint)
{
    wlt_Terminal *term = (wlt_Terminal *)ctx;
    (void)intermediate; (void)nint;
    switch (final)
    {
        case 'M':  /* RI — reverse index */
            if (term->cursor.row == term->scroll_top)
                scroll_down(term, 1);
            else if (term->cursor.row > 0)
                term->cursor.row--;
            break;
        case 'D':  /* IND */
            if (term->cursor.row + 1 >= term->scroll_bottom)
                scroll_up(term, 1);
            else
                term->cursor.row++;
            break;
        case 'E':  /* NEL */
            newline(term);
            break;
        case '7':  /* DECSC */
            term->saved_cursor = term->cursor;
            break;
        case '8':  /* DECRC */
            term->cursor = term->saved_cursor;
            clamp_cursor(term, &term->cursor);
            break;
        case 'c':  /* RIS — full reset */
            term->current_attr = default_attr();
            clear_region(term, 0, 0, term->rows - 1, term->cols - 1);
            term->cursor.row = 0;
            term->cursor.col = 0;
            term->scroll_top = 0;
            term->scroll_bottom = term->rows;
            break;
        default:
            break;
    }
}

static void
on_osc(void *ctx, const char *data, size_t len)
{
    /* OSC strings (window title, color queries) — ignored */
    (void)ctx; (void)data; (void)len;
}

/* ─── Public API ─────────────────────────────────────── */

wlt_terminal_error_t
wlt_terminal_create(wlt_Terminal **out,
                    uint32_t cols,
                    uint32_t rows,
                    const wlt_Config *config)
{
    if (out == NULL)
        return WLT_TERMINAL_ERR_INVALID_ARG;
    if (cols == 0 || rows == 0 ||
        cols > WLT_TERMINAL_MAX_COLS || rows > WLT_TERMINAL_MAX_ROWS)
        return WLT_TERMINAL_ERR_TOO_LARGE;

    wlt_Terminal *term = calloc(1, sizeof(*term));
    if (term == NULL)
        return WLT_TERMINAL_ERR_NOMEM;

    term->cols = cols;
    term->rows = rows;
    term->grid = calloc((size_t)cols * rows, sizeof(wlt_Cell));
    if (term->grid == NULL)
    {
        free(term);
        return WLT_TERMINAL_ERR_NOMEM;
    }

    term->cursor.row = 0;
    term->cursor.col = 0;
    term->cursor.visible = true;
    term->cursor_visible = true;
    term->saved_cursor = term->cursor;
    term->current_attr = default_attr();
    term->scroll_top = 0;
    term->scroll_bottom = rows;
    term->auto_wrap = true;
    term->scroll_offset = 0;

    /* Cursor style + bell from config */
    if (config != NULL)
    {
        term->cursor_style = config->cursor_style;
        term->bell_mode = config->bell;
    }
    else
    {
        term->cursor_style = WLT_CURSOR_BLOCK;
        term->bell_mode = WLT_BELL_NONE;
    }

    wlt_render_init_palette(term, config);

    wlt_terminal_error_t err = wlt_scrollback_create(
        &term->scrollback, WLT_TERMINAL_MAX_SCROLLBACK, cols);
    if (err != WLT_TERMINAL_OK)
    {
        free(term->grid);
        free(term);
        return err;
    }

    wlt_AnsiCallbacks cb = {
        .on_print = on_print,
        .on_execute = on_execute,
        .on_csi = on_csi,
        .on_esc = on_esc,
        .on_osc = on_osc,
    };
    term->parser = calloc(1, sizeof(wlt_AnsiParser));
    if (term->parser == NULL)
    {
        wlt_scrollback_destroy(term->scrollback);
        free(term->grid);
        free(term);
        return WLT_TERMINAL_ERR_NOMEM;
    }
    wlt_ansi_init(term->parser, &cb, term);

    for (uint32_t r = 0; r < rows; r++)
        clear_row(term, r);
    term->full_redraw = true;

    const char *shell = WLT_PTY_DEFAULT_SHELL;
    if (config != NULL && config->shell[0] != '\0')
        shell = config->shell;

    wlt_pty_error_t perr = wlt_pty_create(&term->pty, shell, cols, rows);
    if (perr != WLT_PTY_OK)
    {
        wlt_ansi_free(term->parser);
        free(term->parser);
        wlt_scrollback_destroy(term->scrollback);
        free(term->grid);
        free(term);
        return WLT_TERMINAL_ERR_PTY;
    }

    term->config = (wlt_Config *)config;
    *out = term;
    return WLT_TERMINAL_OK;
}

void
wlt_terminal_destroy(wlt_Terminal *term)
{
    if (term == NULL) return;
    if (term->pty != NULL)
        wlt_pty_destroy(term->pty);
    if (term->parser != NULL)
    {
        wlt_ansi_free(term->parser);
        free(term->parser);
    }
    if (term->scrollback != NULL)
        wlt_scrollback_destroy(term->scrollback);
    free(term->grid);
    free(term);
}

wlt_terminal_error_t
wlt_terminal_output(wlt_Terminal *term,
                    const uint8_t *data,
                    size_t len)
{
    if (term == NULL || (data == NULL && len > 0))
        return WLT_TERMINAL_ERR_INVALID_ARG;
    if (len == 0) return WLT_TERMINAL_OK;
    if (wlt_ansi_feed(term->parser, data, len) != WLT_ANSI_OK)
        return WLT_TERMINAL_ERR_PARSE;
    return WLT_TERMINAL_OK;
}

wlt_terminal_error_t
wlt_terminal_input(wlt_Terminal *term,
                   const uint8_t *data,
                   size_t len)
{
    if (term == NULL || (data == NULL && len > 0))
        return WLT_TERMINAL_ERR_INVALID_ARG;
    if (term->pty == NULL)
        return WLT_TERMINAL_ERR_PTY;
    wlt_pty_error_t err = wlt_pty_write(term->pty, data, len);
    if (err == WLT_PTY_ERR_CLOSED)
        return WLT_TERMINAL_ERR_PTY;
    if (err != WLT_PTY_OK)
        return WLT_TERMINAL_ERR_IO;
    return WLT_TERMINAL_OK;
}

wlt_terminal_error_t
wlt_terminal_resize(wlt_Terminal *term,
                    uint32_t new_cols,
                    uint32_t new_rows)
{
    if (term == NULL)
        return WLT_TERMINAL_ERR_INVALID_ARG;
    if (new_cols == 0 || new_rows == 0 ||
        new_cols > WLT_TERMINAL_MAX_COLS ||
        new_rows > WLT_TERMINAL_MAX_ROWS)
        return WLT_TERMINAL_ERR_TOO_LARGE;
    if (new_cols == term->cols && new_rows == term->rows)
        return WLT_TERMINAL_OK;

    /* Push current grid into scrollback for re-flow */
    for (uint32_t r = 0; r < term->rows; r++)
        wlt_scrollback_push_row(term->scrollback,
                                &term->grid[r * term->cols],
                                term->cols);

    wlt_Cell *new_grid = calloc(
        (size_t)new_cols * new_rows, sizeof(wlt_Cell));
    if (new_grid == NULL)
        return WLT_TERMINAL_ERR_NOMEM;

    free(term->grid);
    term->grid = new_grid;
    term->cols = new_cols;
    term->rows = new_rows;
    term->scroll_top = 0;
    term->scroll_bottom = new_rows;
    term->cursor.row = 0;
    term->cursor.col = 0;
    term->wrap_pending = false;

    for (uint32_t r = 0; r < new_rows; r++)
        clear_row(term, r);
    term->full_redraw = true;

    /* Pull newest lines back from scrollback */
    for (uint32_t r = 0; r < new_rows; r++)
    {
        const wlt_Cell *line = wlt_scrollback_peek(term->scrollback, r);
        if (line == NULL) break;
        memcpy(&term->grid[(new_rows - 1 - r) * new_cols],
               line, new_cols * sizeof(wlt_Cell));
    }

    if (term->pty != NULL)
        wlt_pty_resize(term->pty, new_cols, new_rows);

    return WLT_TERMINAL_OK;
}

wlt_terminal_error_t
wlt_terminal_scroll_view(wlt_Terminal *term, int32_t lines)
{
    if (term == NULL) return WLT_TERMINAL_ERR_INVALID_ARG;
    int32_t off = (int32_t)term->scroll_offset + lines;
    uint32_t max_off = wlt_scrollback_size(term->scrollback);
    if (off < 0) off = 0;
    if ((uint32_t)off > max_off) off = (int32_t)max_off;
    term->scroll_offset = (uint32_t)off;
    term->full_redraw = true;
    return WLT_TERMINAL_OK;
}

void
wlt_terminal_scroll_to_bottom(wlt_Terminal *term)
{
    if (term != NULL)
    {
        term->scroll_offset = 0;
        term->full_redraw = true;
    }
}

wlt_terminal_error_t
wlt_terminal_render(wlt_Terminal *term, wlt_Framebuffer *fb)
{
    if (term == NULL || fb == NULL)
        return WLT_TERMINAL_ERR_INVALID_ARG;
    return wlt_render_draw(term, fb);
}

wlt_terminal_error_t
wlt_terminal_pump(wlt_Terminal *term)
{
    if (term == NULL || term->pty == NULL)
        return WLT_TERMINAL_ERR_INVALID_ARG;

    uint8_t buf[WLT_PTY_READ_BUF];
    size_t n = 0;
    wlt_pty_error_t err = wlt_pty_read(term->pty, buf, sizeof(buf), &n);
    if (err == WLT_PTY_ERR_CLOSED)
        return WLT_TERMINAL_ERR_PTY;
    if (err != WLT_PTY_OK)
        return WLT_TERMINAL_ERR_IO;
    if (n == 0)
        return WLT_TERMINAL_OK;
    return wlt_terminal_output(term, buf, n);
}

void
wlt_terminal_invalidate(wlt_Terminal *term)
{
    if (term == NULL) return;
    for (uint32_t i = 0; i < term->cols * term->rows; i++)
        term->grid[i].dirty = true;
    term->full_redraw = true;
}

void
wlt_terminal_cursor_pos(const wlt_Terminal *term,
                        uint32_t *row, uint32_t *col)
{
    if (term == NULL) return;
    if (row != NULL) *row = term->cursor.row;
    if (col != NULL) *col = term->cursor.col;
}

const char *
wlt_terminal_error_str(wlt_terminal_error_t err)
{
    switch (err)
    {
        case WLT_TERMINAL_OK:               return "ok";
        case WLT_TERMINAL_ERR_INVALID_ARG:  return "invalid argument";
        case WLT_TERMINAL_ERR_NOMEM:        return "out of memory";
        case WLT_TERMINAL_ERR_IO:           return "i/o error";
        case WLT_TERMINAL_ERR_PTY:          return "pty error";
        case WLT_TERMINAL_ERR_NOT_FOUND:    return "not found";
        case WLT_TERMINAL_ERR_PARSE:        return "parse error";
        case WLT_TERMINAL_ERR_TOO_LARGE:    return "too large";
        default:                            return "unknown";
    }
}
