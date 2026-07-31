/**
 * @file wlt-browser.c — WLTIOS Tor Browser v2.0 (FR-APP-10)
 * @purpose A minimal Tor-routed web browser. Fetches pages through
 *          Tor's SOCKS5 proxy via wlt_http_get_tor(), strips HTML,
 *          renders as text. REAL fetch, not a demo string.
 * @dependencies: wlt_http_tor.c, wlt_socks5.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

/* Tor-routed HTTP client (Section D wiring) */
#include "wlt_http_tor.h"

#define MAX_PAGE_SIZE (256 * 1024)
#define MAX_DISPLAY_LINES 50
#define MAX_LINE_LEN 120

static int strip_html(const char *html, size_t len, char *out, size_t out_size) {
    size_t i = 0, j = 0;
    int in_tag = 0, in_script = 0, in_style = 0;
    while (i < len && j < out_size - 1) {
        if (html[i] == '<') {
            if (strncasecmp(html + i, "<script", 7) == 0) in_script = 1;
            if (strncasecmp(html + i, "<style", 6) == 0) in_style = 1;
            in_tag = 1;
        }
        if (!in_tag && !in_script && !in_style) {
            if (html[i] == '&') {
                if (strncasecmp(html + i, "&amp;", 5) == 0) { out[j++] = '&'; i += 5; continue; }
                if (strncasecmp(html + i, "&lt;", 4) == 0) { out[j++] = '<'; i += 4; continue; }
                if (strncasecmp(html + i, "&gt;", 4) == 0) { out[j++] = '>'; i += 4; continue; }
                if (strncasecmp(html + i, "&quot;", 6) == 0) { out[j++] = '"'; i += 6; continue; }
                if (strncasecmp(html + i, "&nbsp;", 6) == 0) { out[j++] = ' '; i += 6; continue; }
            }
            out[j++] = html[i];
        }
        if (html[i] == '>') {
            in_tag = 0;
            if (in_script && i >= 8 && strncasecmp(html + i - 8, "</script>", 9) == 0) in_script = 0;
            if (in_style && i >= 7 && strncasecmp(html + i - 7, "</style>", 8) == 0) in_style = 0;
        }
        i++;
    }
    out[j] = '\0';
    return (int)j;
}

static int format_lines(const char *text, char lines[][MAX_LINE_LEN], int max_lines) {
    int line = 0, col = 0;
    const char *p = text;
    while (*p && line < max_lines) {
        if (*p == '\n' || col >= MAX_LINE_LEN - 1) {
            lines[line][col] = '\0';
            line++; col = 0;
            if (*p == '\n') p++;
        } else if (*p == '\r' || *p == '\t') {
            p++;
        } else {
            lines[line][col++] = *p++;
        }
    }
    if (col > 0 && line < max_lines) { lines[line][col] = '\0'; line++; }
    return line;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "[browser] WLTIOS Tor Browser v2.0\n");
        fprintf(stderr, "[browser] Usage: wlt-browser <url>\n");
        fprintf(stderr, "[browser] Fetches the URL through Tor (127.0.0.1:9050) and\n");
        fprintf(stderr, "[browser] renders it as text. Real Tor routing.\n");
        return 1;
    }

    const char *url = argv[1];
    fprintf(stderr, "[browser] WLTIOS Tor Browser v2.0\n");
    fprintf(stderr, "[browser] Fetching: %s\n", url);
    fprintf(stderr, "[browser] Route: Tor SOCKS5 (127.0.0.1:9050)\n");

    /* REAL FETCH via wlt_http_get_tor() — routes through Tor SOCKS5.
     * No direct connection, no DNS leak. Fail-closed if Tor is down. */
    char raw_response[MAX_PAGE_SIZE];
    int status = wlt_http_get_tor(url, raw_response, sizeof(raw_response), 30);

    if (status < 0) {
        fprintf(stderr, "[browser] Fetch failed (Tor not available or connection error)\n");
        char error_page[512];
        snprintf(error_page, sizeof(error_page),
            "WLTIOS Tor Browser v2.0\n\nERROR: Cannot fetch %s\n\nTor SOCKS5 proxy not available.\nStart Tor first.\n", url);
        char lines[MAX_DISPLAY_LINES][MAX_LINE_LEN];
        int n = format_lines(error_page, lines, MAX_DISPLAY_LINES);
        for (int i = 0; i < n; i++) printf("%4d | %s\n", i, lines[i]);
        return 1;
    }

    fprintf(stderr, "[browser] HTTP status: %d\n", status);
    fprintf(stderr, "[browser] Response: %zu bytes\n", strlen(raw_response));

    /* Strip HTML tags */
    char text[MAX_PAGE_SIZE];
    int text_len = strip_html(raw_response, strlen(raw_response), text, sizeof(text));
    fprintf(stderr, "[browser] After HTML strip: %d bytes\n", text_len);

    /* Format into display lines + render */
    char lines[MAX_DISPLAY_LINES][MAX_LINE_LEN];
    int n = format_lines(text, lines, MAX_DISPLAY_LINES);
    fprintf(stderr, "[browser] %d display lines\n", n);
    for (int i = 0; i < n; i++) printf("%4d | %s\n", i, lines[i]);

    fprintf(stderr, "\n[browser] Page rendered (%d lines) via Tor SOCKS5.\n", n);
    return 0;
}
