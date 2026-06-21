/**
 * @file ansi.h
 * @brief ANSI escape sequence parser (CSI/SGR/OSC) for WLT-Terminal
 * @module wlt-terminal/ansi
 * @purpose Translates a byte stream into terminal state changes
 * @dependencies: terminal.h
 * @tested_by: tests/userspace/ansi_test.c
 * @spec: docs/specs/wlt-terminal.md
 */

#ifndef WLT_ANSI_H
#define WLT_ANSI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ──────────────────────────────────────── */

#define WLT_ANSI_MAX_PARAMS 16
#define WLT_ANSI_MAX_INTERMEDIATE 4
#define WLT_ANSI_STR_INIT_CAP 64

/* ─── Error Codes ────────────────────────────────────── */

typedef enum {
    WLT_ANSI_OK = 0,
    WLT_ANSI_ERR_INVALID_ARG = -1,
    WLT_ANSI_ERR_NOMEM = -2,
    WLT_ANSI_ERR_OVERFLOW = -3,
} wlt_ansi_error_t;

/* ─── Parser State Machine ───────────────────────────── */

/**
 * VT500-style state machine (Paul Williams DEC parser,
 * simplified). Ground -> ESC -> CSI -> dispatch -> Ground.
 */
typedef enum {
    WLT_ANSI_STATE_GROUND = 0,
    WLT_ANSI_STATE_ESC,
    WLT_ANSI_STATE_CSI_ENTRY,
    WLT_ANSI_STATE_CSI_PARAM,
    WLT_ANSI_STATE_CSI_INTERMEDIATE,
    WLT_ANSI_STATE_ESC_INTERMEDIATE,
    WLT_ANSI_STATE_STRING,        /* OSC/DCS body until ST */
} wlt_ansi_state_t;

/* ─── Callbacks ──────────────────────────────────────── */

/**
 * Callbacks invoked by the parser when a complete sequence is
 * recognized. The terminal supplies these.
 */
typedef struct {
    /* A printable character (UTF-8 codepoint) */
    void (*on_print)(void *ctx, uint32_t codepoint);
    /* C0 control character (0x00-0x1F) */
    void (*on_execute)(void *ctx, uint8_t control);
    /* CSI dispatch: final byte + collected params */
    void (*on_csi)(void *ctx, char final,
                   const uint32_t *params, size_t nparams,
                   const char *intermediate, size_t nint,
                   bool private_marker);
    /* ESC dispatch: 0x20-0x2F intermediate + final 0x30-0x7E */
    void (*on_esc)(void *ctx, char final,
                   const char *intermediate, size_t nint);
    /* OSC string (e.g. window title) — not NUL-terminated */
    void (*on_osc)(void *ctx, const char *data, size_t len);
} wlt_AnsiCallbacks;

/* ─── Parser ─────────────────────────────────────────── */

typedef struct wlt_AnsiParser {
    wlt_ansi_state_t state;
    uint32_t params[WLT_ANSI_MAX_PARAMS];
    size_t nparams;
    bool param_active;          /* A digit was seen in current param */

    char intermediate[WLT_ANSI_MAX_INTERMEDIATE];
    size_t nint;
    bool private_marker;        /* CSI started with '?' or '>' */

    /* String accumulation (OSC etc.) */
    char *str_buf;
    size_t str_len;
    size_t str_cap;

    /* UTF-8 decoding */
    uint32_t utf8_codepoint;
    uint32_t utf8_expected;     /* Total bytes in this sequence */
    uint32_t utf8_seen;         /* Bytes seen so far */

    wlt_AnsiCallbacks cb;
    void *ctx;
} wlt_AnsiParser;

/* ─── Public API ─────────────────────────────────────── */

/**
 * Initialize a parser with the given callbacks and user context.
 * The parser does not own `cb` or `ctx`.
 */
wlt_ansi_error_t
wlt_ansi_init(wlt_AnsiParser *parser,
              const wlt_AnsiCallbacks *cb,
              void *ctx);

/**
 * Free internal parser state (string buffer).
 * Safe to call on a zero-initialized parser.
 */
void
wlt_ansi_free(wlt_AnsiParser *parser);

/**
 * Feed bytes into the parser. Callbacks fire as sequences complete.
 */
wlt_ansi_error_t
wlt_ansi_feed(wlt_AnsiParser *parser,
              const uint8_t *data,
              size_t len);

/**
 * Reset parser to ground state (e.g. after RIS).
 */
void
wlt_ansi_reset(wlt_AnsiParser *parser);

const char *
wlt_ansi_error_str(wlt_ansi_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_ANSI_H */
