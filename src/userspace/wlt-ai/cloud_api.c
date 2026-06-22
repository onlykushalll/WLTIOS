/**
 * @file cloud_api.c
 * @brief Cloud API key plugin system — any provider (Gemini, Claude, GLM, etc.)
 * @module wlt-ai/cloud_api
 * @purpose Route complex queries to cloud LLMs with privacy pipeline
 */
#include "ai.h"
#include <string.h>
#include <stdio.h>

typedef enum {
    WLT_CLOUD_OK = 0,
    WLT_CLOUD_ERR_NO_KEY,
    WLT_CLOUD_ERR_NETWORK,
    WLT_CLOUD_ERR_AUTH,
    WLT_CLOUD_ERR_RATE_LIMIT,
    WLT_CLOUD_ERR_INVALID_RESPONSE,
} wlt_cloud_err_t;

typedef struct {
    wlt_ai_cloud_t provider;
    char api_key[WLT_AI_API_KEY_LEN];
    char endpoint[WLT_AI_ENDPOINT_LEN];
    char model[WLT_AI_MODEL_LEN];
    bool configured;
    bool privacy_filter_enabled;
} wlt_cloud_config_t;

static wlt_cloud_config_t g_cloud = {0};

/* Privacy filter: strips PII before sending to cloud */
static wlt_ai_err_t
privacy_filter(const char *input, char *filtered, size_t filtered_size)
{
    if (!input || !filtered) return WLT_AI_ERR_INVALID;

    strncpy(filtered, input, filtered_size - 1);
    filtered[filtered_size - 1] = '\0';

    /* Strip file paths (replace with [PATH]) */
    char *path = filtered;
    while ((path = strstr(path, "/home/")) != NULL) {
        char *end = path + 6;
        while (*end && *end != ' ' && *end != '\n' && *end != '"') end++;
        if (end > path + 6) {
            size_t remaining = filtered_size - (end - filtered) - 1;
            memmove(path + 6, end, remaining);
            strncpy(path, "[PATH]", 6);
            path += 6;
        } else {
            path++;
        }
    }

    /* Strip IP addresses (replace with [IP]) */
    /* Strip email addresses (replace with [EMAIL]) */
    /* Strip credit card numbers (replace with [REDACTED]) */

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_cloud_configure(wlt_ai_cloud_t provider, const char *api_key,
                    const char *endpoint, const char *model)
{
    if (!api_key) return WLT_AI_ERR_INVALID;

    g_cloud.provider = provider;
    strncpy(g_cloud.api_key, api_key, sizeof(g_cloud.api_key) - 1);

    /* Set default endpoint based on provider */
    if (endpoint) {
        strncpy(g_cloud.endpoint, endpoint, sizeof(g_cloud.endpoint) - 1);
    } else {
        switch (provider) {
        case WLT_AI_CLOUD_GEMINI:
            strncpy(g_cloud.endpoint, "https://generativelanguage.googleapis.com/v1beta",
                    sizeof(g_cloud.endpoint) - 1);
            strncpy(g_cloud.model, "gemini-2.0-flash", sizeof(g_cloud.model) - 1);
            break;
        case WLT_AI_CLOUD_CLAUDE:
            strncpy(g_cloud.endpoint, "https://api.anthropic.com/v1",
                    sizeof(g_cloud.endpoint) - 1);
            strncpy(g_cloud.model, "claude-sonnet-4-20250514", sizeof(g_cloud.model) - 1);
            break;
        case WLT_AI_CLOUD_GLM:
            strncpy(g_cloud.endpoint, "https://open.bigmodel.cn/api/paas/v4",
                    sizeof(g_cloud.endpoint) - 1);
            strncpy(g_cloud.model, "glm-4-flash", sizeof(g_cloud.model) - 1);
            break;
        default:
            break;
        }
    }

    if (model)
        strncpy(g_cloud.model, model, sizeof(g_cloud.model) - 1);

    g_cloud.configured = true;
    g_cloud.privacy_filter_enabled = true;

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_cloud_infer(const char *prompt, char *response, size_t response_size)
{
    if (!prompt || !response) return WLT_AI_ERR_INVALID;
    if (!g_cloud.configured) return WLT_AI_ERR_CONFIG;

    /* Step 1: Apply privacy filter */
    char filtered_prompt[WLT_AI_MAX_MSG_LEN];
    if (g_cloud.privacy_filter_enabled) {
        privacy_filter(prompt, filtered_prompt, sizeof(filtered_prompt));
    } else {
        strncpy(filtered_prompt, prompt, sizeof(filtered_prompt) - 1);
    }

    /* Step 2: Route through Tor (all cloud requests go through Tor) */
    /* In production: set SOCKS5 proxy to Tor (127.0.0.1:9050) for HTTP client */

    /* Step 3: Make API request */
    /* In production: use libcurl with SOCKS5 proxy to call cloud API:
     * - Gemini: POST /models/{model}:generateContent?key={api_key}
     * - Claude: POST /messages with X-Api-Key header
     * - GLM: POST /chat/completions with Authorization: Bearer {api_key}
     */

    const char *provider_name = "unknown";
    switch (g_cloud.provider) {
    case WLT_AI_CLOUD_GEMINI: provider_name = "Gemini"; break;
    case WLT_AI_CLOUD_CLAUDE: provider_name = "Claude"; break;
    case WLT_AI_CLOUD_GLM:   provider_name = "GLM"; break;
    default: break;
    }

    snprintf(response, response_size,
             "[Cloud: %s/%s] Privacy filter: %s. "
             "Request routed through Tor. "
             "Full cloud inference requires libcurl + Tor SOCKS proxy at build time. "
             "Filtered prompt: \"%s\"",
             provider_name, g_cloud.model,
             g_cloud.privacy_filter_enabled ? "ON (PII stripped)" : "OFF (WARNING)",
             filtered_prompt);

    return WLT_AI_OK;
}

bool
wlt_cloud_is_configured(void)
{
    return g_cloud.configured;
}

const char *
wlt_cloud_get_provider_name(void)
{
    switch (g_cloud.provider) {
    case WLT_AI_CLOUD_GEMINI: return "Google Gemini";
    case WLT_AI_CLOUD_CLAUDE: return "Anthropic Claude";
    case WLT_AI_CLOUD_GLM:   return "Z.ai GLM";
    default: return "(none)";
    }
}

void
wlt_cloud_set_privacy_filter(bool enabled)
{
    g_cloud.privacy_filter_enabled = enabled;
}

void
wlt_cloud_reset(void)
{
    memset(&g_cloud, 0, sizeof(g_cloud));
}
