/**
 * @file ansi.c
 * @brief ANSI escape sequence parser — VT500-style state machine
 * @module wlt-terminal/ansi
 * @purpose Translates a byte stream into print/execute/csi/esc callbacks
 * @dependencies: ansi.h, terminal.h
 * @tested_by: tests/userspace/ansi_test.c
 * @spec: docs/specs/wlt-terminal.md
 */

#include "ansi.h"
#include "terminal.h"

#include <stdlib.h>
#include <string.h>

/* ─── UTF-8 ──────────────────────────────────────────── */

/**
 * Decode one byte of a UTF-8 sequence. When the sequence is
 * complete, returns the codepoint via *cp and sets *complete.
 * Returns the number of bytes still expected (0 when complete).
 */
static uint32_t
utf8_decode_byte(wlt_AnsiParser *p, uint8_t b,
                 uint32_t *cp, bool *complete)
{
    *complete = false;
    if (p->utf8_expected == 0)
    {
        /* Start of a new sequence */
        if (b < 0x80)
        {
            *cp = b;
            *complete = true;
            return 0;
        }
        if ((b & 0xE0) == 0xC0)
        {
            p->utf8_codepoint = b & 0x1F;
            p->utf8_expected = 2;
            p->utf8_seen = 1;
            return 1;
        }
        if ((b & 0xF0) == 0xE0)
        {
            p->utf8_codepoint = b & 0x0F;
            p->utf8_expected = 3;
            p->utf8_seen = 1;
            return 2;
        }
        if ((b & 0xF8) == 0xF0)
        {
            p->utf8_codepoint = b & 0x07;
            p->utf8_expected = 4;
            p->utf8_seen = 1;
            return 3;
        }
        /* Invalid lead byte — treat as Latin-1 */
        *cp = b;
        *complete = true;
        return 0;
    }
    else
    {
        /* Continuation byte */
        if ((b & 0xC0) != 0x80)
        {
            /* Invalid — reset and emit replacement */
            p->utf8_expected = 0;
            *cp = 0xFFFD;
            *complete = true;
            return 0;
        }
        p->utf8_codepoint = (p->utf8_codepoint << 6) | (b & 0x3F);
        p->utf8_seen++;
        if (p->utf8_seen >= p->utf8_expected)
        {
            *cp = p->utf8_codepoint;
            *complete = true;
            p->utf8_expected = 0;
            p->utf8_seen = 0;
            return 0;
        }
        return p->utf8_expected - p->utf8_seen;
    }
}

/* ─── Param Collection ───────────────────────────────── */

static wlt_ansi_error_t
param_push(wlt_AnsiParser *p)
{
    if (p->nparams >= WLT_ANSI_MAX_PARAMS)
        return WLT_ANSI_ERR_OVERFLOW;
    if (!p->param_active)
    {
        /* Empty param defaults to 0 — caller uses param_or() */
        p->params[p->nparams++] = 0;
    }
    else
    {
        p->nparams++;
    }
    p->param_active = false;
    return WLT_ANSI_OK;
}

static void
param_digit(wlt_AnsiParser *p, uint8_t d)
{
    if (p->nparams == 0)
    {
        /* Implicit first param */
        p->params[0] = 0;
        p->nparams = 1;
    }
    if (!p->param_active)
        p->param_active = true;
    /* Clamp to avoid overflow */
    if (p->params[p->nparams - 1] < 100000)
        p->params[p->nparams - 1] =
            p->params[p->nparams - 1] * 10 + d;
}

static void
intermediate_push(wlt_AnsiParser *p, char c)
{
    if (p->nint < WLT_ANSI_MAX_INTERMEDIATE)
        p->intermediate[p->nint++] = c;
}

/* ─── String (OSC) Accumulation ──────────────────────── */

static wlt_ansi_error_t
str_reserve(wlt_AnsiParser *p, size_t need)
{
    if (p->str_buf != NULL && p->str_cap >= need)
        return WLT_ANSI_OK;

    size_t new_cap = p->str_cap;
    if (new_cap == 0) new_cap = WLT_ANSI_STR_INIT_CAP;
    while (new_cap < need) new_cap *= 2;

    char *nb = realloc(p->str_buf, new_cap);
    if (nb == NULL)
        return WLT_ANSI_ERR_NOMEM;
    p->str_buf = nb;
    p->str_cap = new_cap;
    return WLT_ANSI_OK;
}

static wlt_ansi_error_t
str_append(wlt_AnsiParser *p, char c)
{
    wlt_ansi_error_t err = str_reserve(p, p->str_len + 1);
    if (err != WLT_ANSI_OK) return err;
    p->str_buf[p->str_len++] = c;
    return WLT_ANSI_OK;
}

static void
str_flush(wlt_AnsiParser *p)
{
    if (p->str_buf != NULL && p->str_len > 0 && p->cb.on_osc != NULL)
        p->cb.on_osc(p->ctx, p->str_buf, p->str_len);
    p->str_len = 0;
}

/* ─── Dispatch Helpers ───────────────────────────────── */

static void
ground_byte(wlt_AnsiParser *p, uint8_t b)
{
    bool complete = false;
    uint32_t cp = 0;
    (void)utf8_decode_byte(p, b, &cp, &complete);
    if (complete && p->cb.on_print != NULL && cp >= 0x20)
        p->cb.on_print(p->ctx, cp);
}

static void
dispatch_csi(wlt_AnsiParser *p, char final)
{
    if (p->param_active || p->nparams == 0)
        (void)param_push(p);

    if (p->cb.on_csi != NULL)
        p->cb.on_csi(p->ctx, final,
                     p->params, p->nparams,
                     p->intermediate, p->nint,
                     p->private_marker);
}

static void
dispatch_esc(wlt_AnsiParser *p, char final)
{
    if (p->cb.on_esc != NULL)
        p->cb.on_esc(p->ctx, final, p->intermediate, p->nint);
}

/* ─── State Transitions ──────────────────────────────── */

static void
enter_ground(wlt_AnsiParser *p)
{
    p->state = WLT_ANSI_STATE_GROUND;
    p->nparams = 0;
    p->param_active = false;
    p->nint = 0;
    p->private_marker = false;
}

static void
enter_csi(wlt_AnsiParser *p)
{
    p->state = WLT_ANSI_STATE_CSI_ENTRY;
    p->nparams = 0;
    p->param_active = false;
    p->nint = 0;
    p->private_marker = false;
}

/* ─── Byte Processing ────────────────────────────────── */

static void
process_byte(wlt_AnsiParser *p, uint8_t b)
{
    switch (p->state)
    {
        case WLT_ANSI_STATE_GROUND:
            if (b == 0x1B)             /* ESC */
                p->state = WLT_ANSI_STATE_ESC;
            else if (b < 0x20)         /* C0 control */
            {
                if (p->cb.on_execute != NULL)
                    p->cb.on_execute(p->ctx, b);
            }
            else if (b == 0x7F)        /* DEL — ignore */
                ;
            else
                ground_byte(p, b);
            break;

        case WLT_ANSI_STATE_ESC:
            if (b == '[')
                enter_csi(p);
            else if (b == ']')
            {
                /* OSC: ESC ] ... ST */
                p->state = WLT_ANSI_STATE_STRING;
                p->str_len = 0;
            }
            else if (b >= 0x20 && b <= 0x2F)
            {
                p->intermediate[0] = (char)b;
                p->nint = 1;
                p->state = WLT_ANSI_STATE_ESC_INTERMEDIATE;
            }
            else if (b >= 0x30 && b <= 0x7E)
            {
                dispatch_esc(p, (char)b);
                enter_ground(p);
            }
            else
            {
                /* Unexpected — drop back to ground */
                enter_ground(p);
            }
            break;

        case WLT_ANSI_STATE_ESC_INTERMEDIATE:
            if (b >= 0x20 && b <= 0x2F)
                intermediate_push(p, (char)b);
            else if (b >= 0x30 && b <= 0x7E)
            {
                dispatch_esc(p, (char)b);
                enter_ground(p);
            }
            else
                enter_ground(p);
            break;

        case WLT_ANSI_STATE_CSI_ENTRY:
            if (b >= 0x30 && b <= 0x39)        /* 0-9 */
            {
                p->params[0] = 0;
                p->nparams = 1;
                p->param_active = true;
                param_digit(p, b - '0');
                p->state = WLT_ANSI_STATE_CSI_PARAM;
            }
            else if (b == 0x3B)                 /* ; */
            {
                p->params[0] = 0;
                p->nparams = 1;
                (void)param_push(p);
                p->state = WLT_ANSI_STATE_CSI_PARAM;
            }
            else if (b == 0x3F || b == 0x3E)    /* ? > — private */
            {
                p->private_marker = true;
                p->state = WLT_ANSI_STATE_CSI_PARAM;
            }
            else if (b >= 0x20 && b <= 0x2F)    /* intermediate */
            {
                intermediate_push(p, (char)b);
                p->state = WLT_ANSI_STATE_CSI_INTERMEDIATE;
            }
            else if (b >= 0x40 && b <= 0x7E)    /* final byte */
            {
                dispatch_csi(p, (char)b);
                enter_ground(p);
            }
            else
                enter_ground(p);
            break;

        case WLT_ANSI_STATE_CSI_PARAM:
            if (b >= 0x30 && b <= 0x39)
                param_digit(p, b - '0');
            else if (b == 0x3B)
                (void)param_push(p);
            else if (b >= 0x20 && b <= 0x2F)
            {
                intermediate_push(p, (char)b);
                p->state = WLT_ANSI_STATE_CSI_INTERMEDIATE;
            }
            else if (b >= 0x40 && b <= 0x7E)
            {
                dispatch_csi(p, (char)b);
                enter_ground(p);
            }
            else
                enter_ground(p);
            break;

        case WLT_ANSI_STATE_CSI_INTERMEDIATE:
            if (b >= 0x20 && b <= 0x2F)
                intermediate_push(p, (char)b);
            else if (b >= 0x40 && b <= 0x7E)
            {
                dispatch_csi(p, (char)b);
                enter_ground(p);
            }
            else
                enter_ground(p);
            break;

        case WLT_ANSI_STATE_STRING:
            /* OSC string: terminated by BEL (0x07) or ST (ESC \) */
            if (b == 0x07)
            {
                str_flush(p);
                enter_ground(p);
            }
            else if (b == 0x1B)
            {
                /* Expect '\\' next — peek by switching state.
                 * We treat any ESC inside a string as terminator. */
                str_flush(p);
                enter_ground(p);
                p->state = WLT_ANSI_STATE_ESC;
            }
            else if (b >= 0x20)
            {
                (void)str_append(p, (char)b);
            }
            break;
    }
}

/* ─── Public API ─────────────────────────────────────── */

wlt_ansi_error_t
wlt_ansi_init(wlt_AnsiParser *parser,
              const wlt_AnsiCallbacks *cb,
              void *ctx)
{
    if (parser == NULL || cb == NULL)
        return WLT_ANSI_ERR_INVALID_ARG;

    memset(parser, 0, sizeof(*parser));
    parser->state = WLT_ANSI_STATE_GROUND;
    parser->cb = *cb;
    parser->ctx = ctx;
    parser->str_buf = NULL;
    parser->str_cap = 0;
    parser->str_len = 0;
    parser->utf8_expected = 0;
    parser->utf8_seen = 0;
    return WLT_ANSI_OK;
}

void
wlt_ansi_free(wlt_AnsiParser *parser)
{
    if (parser == NULL) return;
    free(parser->str_buf);
    parser->str_buf = NULL;
    parser->str_cap = 0;
    parser->str_len = 0;
}

wlt_ansi_error_t
wlt_ansi_feed(wlt_AnsiParser *parser,
              const uint8_t *data,
              size_t len)
{
    if (parser == NULL || (data == NULL && len > 0))
        return WLT_ANSI_ERR_INVALID_ARG;
    for (size_t i = 0; i < len; i++)
        process_byte(parser, data[i]);
    return WLT_ANSI_OK;
}

void
wlt_ansi_reset(wlt_AnsiParser *parser)
{
    if (parser == NULL) return;
    parser->state = WLT_ANSI_STATE_GROUND;
    parser->nparams = 0;
    parser->param_active = false;
    parser->nint = 0;
    parser->private_marker = false;
    parser->str_len = 0;
    parser->utf8_expected = 0;
    parser->utf8_seen = 0;
}

const char *
wlt_ansi_error_str(wlt_ansi_error_t err)
{
    switch (err)
    {
        case WLT_ANSI_OK:               return "ok";
        case WLT_ANSI_ERR_INVALID_ARG:  return "invalid argument";
        case WLT_ANSI_ERR_NOMEM:        return "out of memory";
        case WLT_ANSI_ERR_OVERFLOW:     return "overflow";
        default:                        return "unknown";
    }
}
