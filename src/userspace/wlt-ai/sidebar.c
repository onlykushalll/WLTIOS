/**
 * @file sidebar.c
 * @brief Kairos AI sidebar UI — Wayland surface with chat interface
 * @module wlt-ai/sidebar
 * @purpose Floating sidebar for AI chat, anchored to right edge of screen
 * @dependencies: wlt-render, wlt-widgets, wlroots (layer shell)
 * @spec: docs/specs/wlt-ai.md
 */

#include "ai.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define WLT_SIDEBAR_WIDTH       400
#define WLT_SIDEBAR_MAX_MSGS    100
#define WLT_SIDEBAR_MAX_MSG_LEN 4096
#define WLT_SIDEBAR_MAX_INPUT   1024

typedef enum {
    WLT_SIDEBAR_MSG_USER,
    WLT_SIDEBAR_MSG_AI,
    WLT_SIDEBAR_MSG_SYSTEM,
} wlt_sidebar_msg_type_t;

typedef struct {
    wlt_sidebar_msg_type_t type;
    char text[WLT_SIDEBAR_MAX_MSG_LEN];
    uint64_t timestamp;
} wlt_sidebar_msg_t;

typedef struct {
    bool visible;
    bool expanded;
    int32_t width;
    int32_t height;
    wlt_sidebar_msg_t messages[WLT_SIDEBAR_MAX_MSGS];
    size_t msg_count;
    char input_buffer[WLT_SIDEBAR_MAX_INPUT];
    size_t input_len;
    void (*on_send)(const char *text, void *user_data);
    void *user_data;
} wlt_sidebar_state_t;

static wlt_sidebar_state_t g_sidebar = {0};

wlt_ai_err_t
wlt_sidebar_init(void (*on_send)(const char *text, void *user_data), void *user_data)
{
    if (on_send == NULL)
        return WLT_AI_ERR_INVALID;

    memset(&g_sidebar, 0, sizeof(g_sidebar));
    g_sidebar.width = WLT_SIDEBAR_WIDTH;
    g_sidebar.height = 768; /* Default screen height */
    g_sidebar.visible = false;
    g_sidebar.expanded = true;
    g_sidebar.on_send = on_send;
    g_sidebar.user_data = user_data;

    /* Add welcome message */
    g_sidebar.messages[0].type = WLT_SIDEBAR_MSG_SYSTEM;
    strncpy(g_sidebar.messages[0].text,
            "Kairos Online. Say \"Wake up, Buddy!\" or type to begin.",
            WLT_SIDEBAR_MAX_MSG_LEN - 1);
    g_sidebar.msg_count = 1;

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_sidebar_show(void)
{
    g_sidebar.visible = true;
    /* In production: create wlroots layer surface (right edge, overlay layer) */
    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_sidebar_hide(void)
{
    g_sidebar.visible = false;
    /* In production: destroy wlroots layer surface */
    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_sidebar_toggle(void)
{
    if (g_sidebar.visible)
        return wlt_sidebar_hide();
    return wlt_sidebar_show();
}

wlt_ai_err_t
wlt_sidebar_add_message(wlt_sidebar_msg_type_t type, const char *text)
{
    if (text == NULL)
        return WLT_AI_ERR_INVALID;

    if (g_sidebar.msg_count >= WLT_SIDEBAR_MAX_MSGS) {
        /* Scroll: remove oldest message */
        memmove(&g_sidebar.messages[0], &g_sidebar.messages[1],
                (WLT_SIDEBAR_MAX_MSGS - 1) * sizeof(wlt_sidebar_msg_t));
        g_sidebar.msg_count--;
    }

    wlt_sidebar_msg_t *msg = &g_sidebar.messages[g_sidebar.msg_count];
    msg->type = type;
    strncpy(msg->text, text, WLT_SIDEBAR_MAX_MSG_LEN - 1);
    msg->text[WLT_SIDEBAR_MAX_MSG_LEN - 1] = '\0';
    msg->timestamp = 0; /* TODO: set from system clock */
    g_sidebar.msg_count++;

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_sidebar_add_user_message(const char *text)
{
    return wlt_sidebar_add_message(WLT_SIDEBAR_MSG_USER, text);
}

wlt_ai_err_t
wlt_sidebar_add_ai_message(const char *text)
{
    return wlt_sidebar_add_message(WLT_SIDEBAR_MSG_AI, text);
}

wlt_ai_err_t
wlt_sidebar_input_char(char c)
{
    if (c == '\n' || c == '\r') {
        /* Send message */
        if (g_sidebar.input_len > 0) {
            g_sidebar.input_buffer[g_sidebar.input_len] = '\0';
            wlt_sidebar_add_user_message(g_sidebar.input_buffer);
            if (g_sidebar.on_send) {
                g_sidebar.on_send(g_sidebar.input_buffer, g_sidebar.user_data);
            }
            g_sidebar.input_len = 0;
            g_sidebar.input_buffer[0] = '\0';
        }
    } else if (c == 127 || c == 8) {
        /* Backspace */
        if (g_sidebar.input_len > 0) {
            g_sidebar.input_len--;
        }
    } else if (g_sidebar.input_len < WLT_SIDEBAR_MAX_INPUT - 1) {
        g_sidebar.input_buffer[g_sidebar.input_len++] = c;
    }

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_sidebar_render(void *render_ctx)
{
    (void)render_ctx;
    /* In production: use wlt-render to draw the sidebar:
     * - Background (semi-transparent dark panel)
     * - Message history (scrollable)
     * - Input field at bottom
     * - AI status indicator (listening/thinking/idle)
     * - Voice button (microphone icon)
     */
    if (!g_sidebar.visible)
        return WLT_AI_OK;

    return WLT_AI_OK;
}

void
wlt_sidebar_shutdown(void)
{
    memset(&g_sidebar, 0, sizeof(g_sidebar));
}

bool
wlt_sidebar_is_visible(void)
{
    return g_sidebar.visible;
}
