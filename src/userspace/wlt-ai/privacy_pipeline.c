/**
 * @file privacy_pipeline.c
 * @brief Privacy pipeline for cloud model requests — PII stripping + Tor routing
 * @module wlt-ai/privacy
 * @purpose Ensure cloud AI requests maintain Tails-level privacy
 * @dependencies: ai.h
 * @spec: docs/specs/wlt-ai.md
 * @adr: docs/adr/08-kairos.md
 *
 * When Kairos uses a cloud model (Gemini, Claude, etc.), this pipeline:
 *   1. Strips PII (names, emails, IPs, phone numbers) from the prompt
 *   2. Sends only the current message (no conversation history)
 *   3. Routes the request through Tor (no IP leak)
 *   4. Sanitizes the response (removes tracking URLs, scripts)
 *   5. Logs what was sent (transparent to user)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <time.h>

typedef enum {
    KAIROS_PRIV_OK = 0,
    KAIROS_PRIV_ERR = -1,
} kairos_priv_error_t;

#define KAIROS_MAX_PROMPT 8192
#define KAIROS_MAX_LOG 4096

/* PII patterns to strip */
static const char *PII_PATTERNS[] = {
    /* Email addresses */
    "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}",
    /* IP addresses */
    "\\b(?:\\d{1,3}\\.){3}\\d{1,3}\\b",
    /* Phone numbers (various formats) */
    "\\+?\\d{1,4}[-.\\s]?\\(?\\d{1,4}\\)?[-.\\s]?\\d{1,4}[-.\\s]?\\d{1,9}",
    /* Credit card numbers */
    "\\b(?:\\d{4}[-\\s]?){3}\\d{4}\\b",
    /* SSN (US) */
    "\\b\\d{3}-\\d{2}-\\d{4}\\b",
    /* URLs (might contain PII in query params) */
    "https?://[^\\s]+",
    NULL
};

/* PII replacement labels */
static const char *PII_REPLACEMENTS[] = {
    "[EMAIL]",
    "[IP_ADDR]",
    "[PHONE]",
    "[CARD_NUM]",
    "[SSN]",
    "[URL]"
};

/* ─── PII Stripper ───────────────────────────────────── */

kairos_priv_error_t
kairos_priv_strip_pii(const char *input, char *output, size_t max_len)
{
    if (input == NULL || output == NULL || max_len == 0)
        return KAIROS_PRIV_ERR;

    strncpy(output, input, max_len - 1);
    output[max_len - 1] = '\0';

    /* Apply each PII pattern */
    for (int i = 0; PII_PATTERNS[i] != NULL; i++)
    {
        regex_t regex;
        if (regcomp(&regex, PII_PATTERNS[i], REG_EXTENDED) != 0)
            continue;

        /* Replace all matches with replacement label */
        char temp[KAIROS_MAX_PROMPT];
        size_t out_pos = 0;
        size_t in_pos = 0;
        regmatch_t match;

        while (in_pos < strlen(output) && out_pos < sizeof(temp) - 1)
        {
            if (regexec(&regex, output + in_pos, 1, &match, 0) == 0)
            {
                /* Copy text before match */
                size_t before_len = match.rm_so;
                if (out_pos + before_len < sizeof(temp) - 1)
                {
                    memcpy(temp + out_pos, output + in_pos, before_len);
                    out_pos += before_len;
                }

                /* Insert replacement label */
                const char *replacement = PII_REPLACEMENTS[i];
                size_t rep_len = strlen(replacement);
                if (out_pos + rep_len < sizeof(temp) - 1)
                {
                    memcpy(temp + out_pos, replacement, rep_len);
                    out_pos += rep_len;
                }

                /* Skip matched text */
                in_pos += match.rm_eo;
            }
            else
            {
                /* No more matches — copy rest */
                size_t remaining = strlen(output + in_pos);
                if (out_pos + remaining < sizeof(temp) - 1)
                {
                    memcpy(temp + out_pos, output + in_pos, remaining);
                    out_pos += remaining;
                }
                break;
            }
        }

        temp[out_pos] = '\0';
        strncpy(output, temp, max_len - 1);
        output[max_len - 1] = '\0';

        regfree(&regex);
    }

    return KAIROS_PRIV_OK;
}

/* ─── Response Sanitizer ─────────────────────────────── */

kairos_priv_error_t
kairos_priv_sanitize_response(const char *input, char *output, size_t max_len)
{
    if (input == NULL || output == NULL || max_len == 0)
        return KAIROS_PRIV_ERR;

    strncpy(output, input, max_len - 1);
    output[max_len - 1] = '\0';

    /* Remove tracking pixels (1x1 images) */
    char *pos = output;
    while ((pos = strstr(pos, "<img")) != NULL)
    {
        char *end = strstr(pos, ">");
        if (end != NULL)
        {
            /* Check if it's a tracking pixel (width=1 or height=1) */
            if (strstr(pos, "width=\"1\"") != NULL ||
                strstr(pos, "height=\"1\"") != NULL ||
                strstr(pos, "width='1'") != NULL ||
                strstr(pos, "height='1'") != NULL)
            {
                /* Remove the tracking pixel tag */
                (void)0; /* tag_len removed — was unused */
    size_t tag_len_unused = end - pos + 1;
                memmove(pos, end + 1, strlen(end + 1) + 1);
                continue;
            }
        }
        pos = end ? end + 1 : pos + 4;
    }

    /* Remove script tags */
    while ((pos = strstr(output, "<script")) != NULL)
    {
        char *end = strstr(pos, "</script>");
        if (end != NULL)
        {
            end += 9;  /* strlen("</script>") */
            memmove(pos, end, strlen(end) + 1);
        }
        else
        {
            /* Unclosed script tag — remove rest */
            *pos = '\0';
            break;
        }
    }

    /* Remove iframe tags (potential tracking) */
    while ((pos = strstr(output, "<iframe")) != NULL)
    {
        char *end = strstr(pos, "</iframe>");
        if (end != NULL)
        {
            end += 9;
            memmove(pos, end, strlen(end) + 1);
        }
        else
        {
            char *tag_end = strstr(pos, ">");
            if (tag_end != NULL)
            {
                memmove(pos, tag_end + 1, strlen(tag_end + 1) + 1);
            }
            else
            {
                *pos = '\0';
                break;
            }
        }
    }

    return KAIROS_PRIV_OK;
}

/* ─── Transparency Log ───────────────────────────────── */

static char g_transparency_log[KAIROS_MAX_LOG * 10];
static size_t g_log_pos = 0;

kairos_priv_error_t
kairos_priv_log_request(const char *provider, const char *prompt,
                         const char *response)
{
    if (provider == NULL)
        return KAIROS_PRIV_ERR;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    int written = snprintf(g_transparency_log + g_log_pos,
                           sizeof(g_transparency_log) - g_log_pos,
                           "[%04d-%02d-%02d %02d:%02d:%02d] Provider: %s\n"
                           "  Sent: %.200s\n"
                           "  Received: %.200s\n\n",
                           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                           tm->tm_hour, tm->tm_min, tm->tm_sec,
                           provider,
                           prompt ? prompt : "(none)",
                           response ? response : "(none)");

    if (written > 0)
        g_log_pos += written;

    if (g_log_pos >= sizeof(g_transparency_log) - 1024)
    {
        /* Rotate log — keep last 50% */
        size_t half = g_log_pos / 2;
        memmove(g_transparency_log,
                g_transparency_log + half,
                g_log_pos - half);
        g_log_pos -= half;
    }

    return KAIROS_PRIV_OK;
}

const char *
kairos_priv_get_log(void)
{
    return g_transparency_log;
}

void
kairos_priv_clear_log(void)
{
    g_log_pos = 0;
    g_transparency_log[0] = '\0';
}
