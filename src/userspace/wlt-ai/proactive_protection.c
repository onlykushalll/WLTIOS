/**
 * @file proactive_protection.c
 * @brief Proactive protection — monitors system, warns user, fights attacks
 * @module wlt-ai/proactive_protection
 * @purpose JARVIS-style active defense: detect attacks, warn user, take action
 */
#include "ai.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#define WLT_PP_MAX_EVENTS 100
#define WLT_PP_IDLE_THRESHOLD_SEC 300  /* 5 minutes */
#define WLT_PP_SCAN_THRESHOLD_SEC 60   /* Scan within 60s = suspicious */

/* wlt_pp_event_type_t declared in ai.h */

typedef struct {
    wlt_pp_event_type_t type;
    time_t timestamp;
    char detail[256];
    bool resolved;
} wlt_pp_event_t;

typedef struct {
    wlt_pp_event_t events[WLT_PP_MAX_EVENTS];
    size_t event_count;
    time_t last_user_activity;
    bool screen_locked;
    bool monitoring;
    void (*on_alert)(wlt_pp_event_type_t type, const char *detail, void *user);
    void *user_data;
} wlt_pp_state_t;

static wlt_pp_state_t g_pp = {0};

static const char *
event_str(wlt_pp_event_type_t t)
{
    switch (t) {
    case WLT_PP_EVENT_PORT_SCAN:           return "Port scan detected";
    case WLT_PP_EVENT_BRUTE_FORCE:         return "Brute force attack";
    case WLT_PP_EVENT_SUSPICIOUS_PROCESS:  return "Suspicious process";
    case WLT_PP_EVENT_DISK_ENCRYPTION_OFF: return "Disk encryption disabled";
    case WLT_PP_EVENT_TOR_DISCONNECTED:    return "Tor disconnected";
    case WLT_PP_EVENT_NEW_USB_DEVICE:      return "New USB device connected";
    case WLT_PP_EVENT_USER_IDLE:           return "User inactive";
    case WLT_PP_EVENT_SCREEN_LOCK_OFF:     return "Screen lock disabled";
    default: return "Unknown event";
    }
}

wlt_ai_err_t
wlt_pp_init(void (*on_alert)(wlt_pp_event_type_t, const char *, void *), void *user_data)
{
    if (!on_alert) return WLT_AI_ERR_INVALID;
    memset(&g_pp, 0, sizeof(g_pp));
    g_pp.on_alert = on_alert;
    g_pp.user_data = user_data;
    g_pp.last_user_activity = time(NULL);
    g_pp.monitoring = true;
    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_pp_report_event(wlt_pp_event_type_t type, const char *detail)
{
    if (g_pp.event_count >= WLT_PP_MAX_EVENTS) {
        memmove(&g_pp.events[0], &g_pp.events[1],
                (WLT_PP_MAX_EVENTS - 1) * sizeof(wlt_pp_event_t));
        g_pp.event_count--;
    }

    wlt_pp_event_t *e = &g_pp.events[g_pp.event_count];
    e->type = type;
    e->timestamp = time(NULL);
    e->resolved = false;
    if (detail)
        strncpy(e->detail, detail, sizeof(e->detail) - 1);
    else
        e->detail[0] = '\0';
    g_pp.event_count++;

    /* Trigger alert callback */
    if (g_pp.on_alert)
        g_pp.on_alert(type, e->detail, g_pp.user_data);

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_pp_user_activity(void)
{
    g_pp.last_user_activity = time(NULL);
    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_pp_check_idle(void)
{
    time_t now = time(NULL);
    if (now - g_pp.last_user_activity > WLT_PP_IDLE_THRESHOLD_SEC) {
        if (!g_pp.screen_locked) {
            wlt_pp_report_event(WLT_PP_EVENT_USER_IDLE,
                "User has been inactive for more than 5 minutes. "
                "Locking screen for security.");
            return WLT_AI_OK;
        }
    }
    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_pp_get_report(char *report, size_t report_size)
{
    if (!report) return WLT_AI_ERR_INVALID;

    size_t offset = 0;
    offset += snprintf(report + offset, report_size - offset,
                       "=== Proactive Protection Report ===\n\n");

    if (g_pp.event_count == 0) {
        offset += snprintf(report + offset, report_size - offset,
                           "No security events detected. System is secure.\n");
        return WLT_AI_OK;
    }

    offset += snprintf(report + offset, report_size - offset,
                       "%zu security event(s) recorded:\n\n", g_pp.event_count);

    for (size_t i = 0; i < g_pp.event_count && offset < report_size - 200; i++) {
        wlt_pp_event_t *e = &g_pp.events[i];
        offset += snprintf(report + offset, report_size - offset,
                           "  [%s] %s — %s\n",
                           e->resolved ? "RESOLVED" : "ACTIVE",
                           event_str(e->type),
                           e->detail[0] ? e->detail : "no details");
    }

    return WLT_AI_OK;
}

void
wlt_pp_resolve_event(size_t index)
{
    if (index < g_pp.event_count)
        g_pp.events[index].resolved = true;
}

void
wlt_pp_shutdown(void)
{
    memset(&g_pp, 0, sizeof(g_pp));
}
