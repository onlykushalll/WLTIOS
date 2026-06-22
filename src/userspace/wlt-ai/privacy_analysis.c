/**
 * @file privacy_analysis.c
 * @brief Privacy analysis — "which apps are spying on me?"
 * @module wlt-ai/privacy_analysis
 * @purpose Analyze app permissions, network connections, file access patterns
 */
#include "ai.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define WLT_PA_MAX_APPS 64
#define WLT_PA_MAX_NAME 128

typedef enum {
    WLT_PA_RISK_SAFE = 0,       /* No network, sandboxed */
    WLT_PA_RISK_LOW,            /* Tor-only network, limited files */
    WLT_PA_RISK_MEDIUM,         /* Direct network or broad file access */
    WLT_PA_RISK_HIGH,           /* Camera/mic + network + file access */
    WLT_PA_RISK_CRITICAL,       /* Background data collection, keylogger-like */
} wlt_pa_risk_t;

typedef struct {
    char name[WLT_PA_MAX_NAME];
    bool has_network;
    bool has_direct_network;     /* Non-Tor */
    bool has_file_access;
    bool has_camera;
    bool has_mic;
    bool has_location;
    bool runs_in_background;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    wlt_pa_risk_t risk;
} wlt_pa_app_t;

typedef struct {
    wlt_pa_app_t apps[WLT_PA_MAX_APPS];
    size_t count;
} wlt_pa_analysis_t;

static wlt_pa_analysis_t g_analysis = {0};

static const char *
risk_str(wlt_pa_risk_t r)
{
    switch (r) {
    case WLT_PA_RISK_SAFE:     return "Safe";
    case WLT_PA_RISK_LOW:      return "Low";
    case WLT_PA_RISK_MEDIUM:   return "Medium";
    case WLT_PA_RISK_HIGH:     return "High";
    case WLT_PA_RISK_CRITICAL: return "CRITICAL";
    default:                   return "Unknown";
    }
}

static const char *
risk_emoji(wlt_pa_risk_t r)
{
    switch (r) {
    case WLT_PA_RISK_SAFE:     return "[SAFE]";
    case WLT_PA_RISK_LOW:      return "[LOW]";
    case WLT_PA_RISK_MEDIUM:   return "[MED]";
    case WLT_PA_RISK_HIGH:     return "[HIGH]";
    case WLT_PA_RISK_CRITICAL: return "[CRIT]";
    default:                   return "[?]";
    }
}

static wlt_pa_risk_t
calculate_risk(const wlt_pa_app_t *app)
{
    if (app->has_direct_network && (app->has_camera || app->has_mic))
        return WLT_PA_RISK_CRITICAL;
    if (app->has_direct_network)
        return WLT_PA_RISK_HIGH;
    if ((app->has_camera || app->has_mic) && app->has_network)
        return WLT_PA_RISK_HIGH;
    if (app->has_network && app->has_file_access)
        return WLT_PA_RISK_MEDIUM;
    if (app->has_network)
        return WLT_PA_RISK_LOW;
    return WLT_PA_RISK_SAFE;
}

wlt_ai_err_t
wlt_ai_privacy_analyze(char *report, size_t report_size)
{
    if (!report || report_size == 0)
        return WLT_AI_ERR_INVALID;

    /* In production: read from /proc/net to check connections,
     * read app manifests from /etc/wlt/apps/, check permissions DB.
     * For now: use the registered apps in g_analysis. */

    size_t offset = 0;
    offset += snprintf(report + offset, report_size - offset,
                       "=== Privacy Analysis Report ===\n\n");

    int safe = 0, low = 0, med = 0, high = 0, crit = 0;
    for (size_t i = 0; i < g_analysis.count; i++) {
        wlt_pa_app_t *a = &g_analysis.apps[i];
        a->risk = calculate_risk(a);

        const char *emoji = risk_emoji(a->risk);
        offset += snprintf(report + offset, report_size - offset,
                           "  %s %s — risk: %s", emoji, a->name, risk_str(a->risk));

        if (a->has_direct_network)
            offset += snprintf(report + offset, report_size - offset, " [DIRECT NET!]");
        if (a->has_camera)
            offset += snprintf(report + offset, report_size - offset, " [CAMERA]");
        if (a->has_mic)
            offset += snprintf(report + offset, report_size - offset, " [MIC]");
        if (a->runs_in_background)
            offset += snprintf(report + offset, report_size - offset, " [BACKGROUND]");

        offset += snprintf(report + offset, report_size - offset, "\n");

        switch (a->risk) {
        case WLT_PA_RISK_SAFE:     safe++; break;
        case WLT_PA_RISK_LOW:      low++; break;
        case WLT_PA_RISK_MEDIUM:   med++; break;
        case WLT_PA_RISK_HIGH:     high++; break;
        case WLT_PA_RISK_CRITICAL: crit++; break;
        }
    }

    offset += snprintf(report + offset, report_size - offset,
                       "\nSummary: %d safe, %d low, %d medium, %d high, %d critical\n",
                       safe, low, med, high, crit);

    if (crit > 0 || high > 0) {
        offset += snprintf(report + offset, report_size - offset,
                           "\nWARNING: %d app(s) have high or critical privacy risk!\n",
                           high + crit);
        offset += snprintf(report + offset, report_size - offset,
                           "Recommend: Review permissions in Settings > Privacy\n");
    }

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_ai_privacy_register_app(const char *name, bool network, bool direct_net,
                            bool files, bool camera, bool mic, bool background)
{
    if (!name || g_analysis.count >= WLT_PA_MAX_APPS)
        return WLT_AI_ERR_INVALID;

    wlt_pa_app_t *a = &g_analysis.apps[g_analysis.count];
    memset(a, 0, sizeof(*a));
    strncpy(a->name, name, WLT_PA_MAX_NAME - 1);
    a->has_network = network;
    a->has_direct_network = direct_net;
    a->has_file_access = files;
    a->has_camera = camera;
    a->has_mic = mic;
    a->runs_in_background = background;
    a->risk = calculate_risk(a);
    g_analysis.count++;

    return WLT_AI_OK;
}

void
wlt_ai_privacy_reset(void)
{
    memset(&g_analysis, 0, sizeof(g_analysis));
}
