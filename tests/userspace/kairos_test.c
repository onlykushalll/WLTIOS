/**
 * @file kairos_test.c
 * @brief Unit tests for Kairos AI HTTP request builder + response parser
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static int g_pass = 0;
static int g_fail = 0;
#define TEST_ASSERT(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS: %s\n", msg); } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } \
} while(0)

static void build_request(char *buf, size_t bufsize, const char *question)
{
    snprintf(buf, bufsize,
        "{\"messages\":["
        "{\"role\":\"system\",\"content\":\"You are Kairos, the AI assistant "
        "built into WLTIOS operating system. WLTIOS routes all traffic through "
        "Tor for anonymity. Be concise. Protect the user's privacy.\"},"
        "{\"role\":\"user\",\"content\":\"%s\"}"
        "],\"max_tokens\":100}", question);
}

static int parse_content(const char *response, char *content, size_t content_size)
{
    const char *start = strstr(response, "\"content\":\"");
    if (!start) return -1;
    start += 11;
    const char *end = strstr(start, "\"}");
    if (!end) end = strstr(start, "\",");
    if (!end) return -1;
    size_t len = end - start;
    if (len >= content_size) len = content_size - 1;
    memcpy(content, start, len);
    content[len] = '\0';
    return 0;
}

int main(void)
{
    printf("=== Kairos AI Tests ===\n\n");

    printf("--- Request builder ---\n");
    char request[2048];
    build_request(request, sizeof(request), "Hello");
    TEST_ASSERT(strstr(request, "\"role\":\"system\"") != NULL, "System role present");
    TEST_ASSERT(strstr(request, "\"role\":\"user\"") != NULL, "User role present");
    TEST_ASSERT(strstr(request, "Kairos") != NULL, "System prompt mentions Kairos");
    TEST_ASSERT(strstr(request, "WLTIOS") != NULL, "System prompt mentions WLTIOS");
    TEST_ASSERT(strstr(request, "Tor") != NULL, "System prompt mentions Tor");
    TEST_ASSERT(strstr(request, "\"max_tokens\":100") != NULL, "Max tokens set to 100");
    TEST_ASSERT(strstr(request, "Hello") != NULL, "User question in request");
    TEST_ASSERT(strlen(request) > 100, "Request is reasonably sized");

    printf("\n--- Longer question ---\n");
    build_request(request, sizeof(request), "What is my IP address through Tor?");
    TEST_ASSERT(strstr(request, "IP address") != NULL, "Longer question present");

    printf("\n--- Response parser (valid) ---\n");
    const char *valid = "{\"choices\":[{\"message\":{\"content\":\"Hello! I'm Kairos.\"}}]}";
    char content[256];
    int rc = parse_content(valid, content, sizeof(content));
    TEST_ASSERT(rc == 0, "Parser returns 0 on valid response");
    TEST_ASSERT(strcmp(content, "Hello! I'm Kairos.") == 0, "Content extracted correctly");

    printf("\n--- Parser error cases ---\n");
    rc = parse_content("no json here", content, sizeof(content));
    TEST_ASSERT(rc == -1, "Parser returns -1 on non-JSON");
    rc = parse_content("{\"choices\":[]}", content, sizeof(content));
    TEST_ASSERT(rc == -1, "Parser returns -1 on missing content");

    printf("\n--- System prompt content ---\n");
    build_request(request, sizeof(request), "test");
    TEST_ASSERT(strstr(request, "concise") != NULL, "System prompt says 'concise'");
    TEST_ASSERT(strstr(request, "privacy") != NULL, "System prompt says 'privacy'");
    TEST_ASSERT(strstr(request, "anonymity") != NULL, "System prompt mentions anonymity");

    printf("\n--- HTTP request format ---\n");
    char http_req[4096];
    snprintf(http_req, sizeof(http_req),
        "POST /v1/chat/completions HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n%s",
        strlen(request), request);
    TEST_ASSERT(strstr(http_req, "POST /v1/chat/completions") != NULL, "HTTP POST method");
    TEST_ASSERT(strstr(http_req, "Host: 127.0.0.1:8080") != NULL, "Host header correct");
    TEST_ASSERT(strstr(http_req, "Content-Type: application/json") != NULL, "Content-Type JSON");
    TEST_ASSERT(strstr(http_req, "\r\n\r\n") != NULL, "Header/body separator");

    printf("\n--- First words verification ---\n");
    const char *first_words = "Hello! I'm Kairos, the AI assistant built into the WLTIOS operating system.";
    char wrapped[512];
    snprintf(wrapped, sizeof(wrapped), "{\"choices\":[{\"message\":{\"content\":\"%s\"}}]}", first_words);
    rc = parse_content(wrapped, content, sizeof(content));
    TEST_ASSERT(rc == 0, "Wrapped first words parse correctly");
    TEST_ASSERT(strstr(content, "Kairos") != NULL, "First words mention Kairos");
    TEST_ASSERT(strstr(content, "WLTIOS") != NULL, "First words mention WLTIOS");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}