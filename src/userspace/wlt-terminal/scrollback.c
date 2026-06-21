/**
 * @file scrollback.c
 * @brief Circular scrollback buffer for WLT-Terminal
 * @module wlt-terminal/scrollback
 * @purpose Stores up to 10000 lines that scrolled off the top of the grid
 * @dependencies: terminal.h
 * @tested_by: tests/userspace/scrollback_test.c
 * @spec: docs/specs/wlt-terminal.md
 */

#include "terminal.h"

#include <stdlib.h>
#include <string.h>

/* ─── Scrollback State ───────────────────────────────── */

/**
 * Circular buffer of fixed-width rows. Each row holds `cols` cells.
 * `head` is the index of the oldest row; `count` is how many rows
 * are currently stored. New rows overwrite the oldest when full.
 */
struct wlt_Scrollback {
    wlt_Cell *rows;         /* capacity * cols cells, flat */
    uint32_t capacity;      /* Max rows (typically 10000) */
    uint32_t cols;          /* Cells per row */
    uint32_t head;          /* Index of oldest row */
    uint32_t count;         /* Rows currently stored */
};

/* ─── Helpers ────────────────────────────────────────── */

static inline wlt_Cell *
row_ptr(wlt_Scrollback *sb, uint32_t idx)
{
    return &sb->rows[(size_t)idx * sb->cols];
}

static inline const wlt_Cell *
row_ptr_const(const wlt_Scrollback *sb, uint32_t idx)
{
    return &sb->rows[(size_t)idx * sb->cols];
}

/* ─── Public API ─────────────────────────────────────── */

wlt_terminal_error_t
wlt_scrollback_create(wlt_Scrollback **out,
                      uint32_t capacity,
                      uint32_t cols)
{
    if (out == NULL || capacity == 0 || cols == 0)
        return WLT_TERMINAL_ERR_INVALID_ARG;

    /* Cap to the spec limit to keep RAM under budget */
    if (capacity > WLT_TERMINAL_MAX_SCROLLBACK)
        capacity = WLT_TERMINAL_MAX_SCROLLBACK;

    wlt_Scrollback *sb = calloc(1, sizeof(*sb));
    if (sb == NULL)
        return WLT_TERMINAL_ERR_NOMEM;

    /* Overflow guard: cap total cell count to a sensible ceiling */
    if (capacity > 100000 || cols > 1024)
    {
        free(sb);
        return WLT_TERMINAL_ERR_TOO_LARGE;
    }

    sb->rows = calloc((size_t)capacity * cols, sizeof(wlt_Cell));
    if (sb->rows == NULL)
    {
        free(sb);
        return WLT_TERMINAL_ERR_NOMEM;
    }

    sb->capacity = capacity;
    sb->cols = cols;
    sb->head = 0;
    sb->count = 0;
    *out = sb;
    return WLT_TERMINAL_OK;
}

void
wlt_scrollback_destroy(wlt_Scrollback *sb)
{
    if (sb == NULL) return;
    free(sb->rows);
    free(sb);
}

/**
 * Append a row. If full, the oldest row is discarded (overwritten).
 * Caller passes a buffer of at least `cols` cells. If the buffer is
 * wider/narrower than our row width, we copy the minimum.
 */
void
wlt_scrollback_push_row(wlt_Scrollback *sb,
                        const wlt_Cell *cells,
                        uint32_t cols)
{
    if (sb == NULL || cells == NULL) return;

    /* Target write position. When the buffer is full we overwrite
     * the oldest row (which is at `head`) and advance head. */
    uint32_t write_idx;
    if (sb->count < sb->capacity)
    {
        write_idx = sb->head + sb->count;
        if (write_idx >= sb->capacity) write_idx -= sb->capacity;
        sb->count++;
    }
    else
    {
        write_idx = sb->head;
        sb->head++;
        if (sb->head >= sb->capacity) sb->head = 0;
    }

    wlt_Cell *dst = row_ptr(sb, write_idx);
    uint32_t to_copy = (cols < sb->cols) ? cols : sb->cols;
    memcpy(dst, cells, (size_t)to_copy * sizeof(wlt_Cell));

    /* If the source row was narrower, pad with spaces */
    if (to_copy < sb->cols)
    {
        for (uint32_t i = to_copy; i < sb->cols; i++)
        {
            dst[i].codepoint = ' ';
            dst[i].fg = 7;
            dst[i].bg = 0;
            dst[i].bold = false;
            dst[i].italic = false;
            dst[i].underline = false;
            dst[i].reverse = false;
            dst[i].blink = false;
            dst[i].dirty = false;
        }
    }
}

/**
 * Peek at the row `from_end` rows back from the most recent push.
 *   from_end == 0 -> most recently pushed row
 *   from_end == 1 -> the row before that
 * Returns NULL if from_end >= count.
 */
const wlt_Cell *
wlt_scrollback_peek(wlt_Scrollback *sb, uint32_t from_end)
{
    if (sb == NULL || from_end >= sb->count) return NULL;

    /* Most-recent row is at (head + count - 1) % capacity */
    uint32_t idx = (sb->head + sb->count - 1 - from_end);
    if (idx >= sb->capacity)
        idx -= sb->capacity;
    if (idx >= sb->capacity)  /* defensive: wrapped twice */
        return NULL;
    return row_ptr_const(sb, idx);
}

uint32_t
wlt_scrollback_size(wlt_Scrollback *sb)
{
    if (sb == NULL) return 0;
    return sb->count;
}
