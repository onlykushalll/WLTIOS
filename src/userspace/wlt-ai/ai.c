/*
 * ai.c - WLT-AI Main AI Engine
 *
 * Core engine for the WLTIOS built-in AI assistant. Handles LLM
 * integration (local llama.cpp server or cloud API), tool-use
 * dispatch, and conversation memory management.
 *
 * The engine receives user queries (text or voice), constructs a
 * conversation context from history, sends it to the LLM, parses
 * any embedded tool calls, executes them, and returns the response.
 *
 * Dependencies: libcurl (HTTP), libc, POSIX
 *
 * Copyright (c) WLTIOS Project. All rights reserved.
 */

#include "ai.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include <curl/curl.h>

/* ------------------------------------------------------------------ */
/* Internal types                                                     */
/* ------------------------------------------------------------------ */

/* Dynamic buffer for HTTP response data */
typedef struct {
    char    *data;
    size_t   size;
    size_t   capacity;
} wlt_buf_t;

/* AI engine context */
struct wlt_ai_ctx {
    wlt_ai_config_t     config;
    wlt_ai_history_t    history;
    CURL               *curl;
    bool                initialized;
};

/* ------------------------------------------------------------------ */
/* Forward declarations (internal)                                    */
/* ------------------------------------------------------------------ */

static wlt_ai_err_t wlt_llm_call_local(wlt_ai_ctx_t *ctx,
                                       const wlt_ai_history_t *history,
                                       char *response, size_t resp_len);

static wlt_ai_err_t wlt_llm_call_cloud(wlt_ai_ctx_t *ctx,
                                       const wlt_ai_history_t *history,
                                       char *response, size_t resp_len);

static wlt_ai_err_t wlt_stt_transcribe(wlt_ai_ctx_t *ctx,
                                       const uint8_t *audio,
                                       size_t audio_len,
                                       char *text, size_t text_len);

static wlt_ai_err_t wlt_tts_speak(wlt_ai_ctx_t *ctx, const char *text);

static void wlt_parse_tool_call(const char *llm_output,
                                wlt_ai_response_t *resp);

static size_t wlt_curl_write_cb(void *ptr, size_t size,
                                size_t nmemb, void *userdata);

static int wlt_buf_append(wlt_buf_t *buf, const char *data, size_t len);

static void wlt_buf_free(wlt_buf_t *buf);

static const char *wlt_role_str(wlt_ai_role_t role);

static void wlt_json_escape(char *out, size_t out_len, const char *in);

static char *wlt_extract_json_field(const char *json,
                                    const char *field,
                                    char *out, size_t out_len);

static void wlt_build_chat_json(const wlt_ai_history_t *history,
                                const char *system_prompt,
                                char *json, size_t json_len);

/* ------------------------------------------------------------------ */
/* Engine lifecycle                                                   */
/* ------------------------------------------------------------------ */

wlt_ai_ctx_t *
wlt_ai_init(const wlt_ai_config_t *cfg)
{
    wlt_ai_ctx_t *ctx;

    if (cfg == NULL || !cfg->enabled) {
        fprintf(stderr, "wlt-ai: disabled or null config\n");
        return NULL;
    }

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        fprintf(stderr, "wlt-ai: out of memory\n");
        return NULL;
    }

    memcpy(&ctx->config, cfg, sizeof(ctx->config));
    ctx->history.count = 0;
    ctx->history.head = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    ctx->curl = curl_easy_init();
    if (ctx->curl == NULL) {
        fprintf(stderr, "wlt-ai: curl init failed\n");
        free(ctx);
        return NULL;
    }

    /* Load persistent memory if configured */
    if (ctx->config.memory_mode == WLT_AI_MEMORY_PERSISTENT) {
        wlt_ai_err_t err = wlt_memory_init(WLT_AI_MEMORY_PATH,
                                           ctx->config.encrypt_key,
                                           WLT_AI_ENCRYPT_KEY_LEN);
        if (err == WLT_AI_OK) {
            wlt_memory_load(&ctx->history);
        }
        /* Non-fatal: start with empty history on load failure */
    }

    /* System prompt */
    wlt_memory_add_message(&ctx->history, WLT_AI_ROLE_SYSTEM,
        "You are WLT-AI, the built-in assistant for WLTIOS. "
        "You can control the OS by emitting tool calls in the format "
        "[TOOL: open_app|change_setting|run_command|file_search|"
        "screen_analyze args=\"...\"]");

    ctx->initialized = true;
    return ctx;
}

void
wlt_ai_shutdown(wlt_ai_ctx_t *ctx)
{
    if (ctx == NULL)
        return;

    if (ctx->config.memory_mode == WLT_AI_MEMORY_PERSISTENT) {
        wlt_memory_save(&ctx->history);
    } else {
        /* Amnesic mode: wipe everything */
        wlt_memory_clear();
    }

    if (ctx->curl) {
        curl_easy_cleanup(ctx->curl);
    }
    curl_global_cleanup();

    memset(ctx, 0, sizeof(*ctx));
    free(ctx);
}

/* ------------------------------------------------------------------ */
/* Query processing                                                   */
/* ------------------------------------------------------------------ */

wlt_ai_response_t
wlt_ai_query(wlt_ai_ctx_t *ctx, const char *query)
{
    wlt_ai_response_t resp;
    wlt_ai_err_t err;

    memset(&resp, 0, sizeof(resp));

    if (ctx == NULL || !ctx->initialized || query == NULL) {
        resp.error = WLT_AI_ERR_INVALID;
        return resp;
    }

    /* Record the user message */
    wlt_memory_add_message(&ctx->history, WLT_AI_ROLE_USER, query);

    /* Call the LLM */
    if (ctx->config.mode == WLT_AI_MODE_LOCAL) {
        err = wlt_llm_call_local(ctx, &ctx->history,
                                 resp.text, sizeof(resp.text));
    } else if (ctx->config.mode == WLT_AI_MODE_CLOUD) {
        err = wlt_llm_call_cloud(ctx, &ctx->history,
                                 resp.text, sizeof(resp.text));
    } else {
        /* Hybrid: try local first, fall back to cloud */
        err = wlt_llm_call_local(ctx, &ctx->history,
                                 resp.text, sizeof(resp.text));
        if (err != WLT_AI_OK) {
            err = wlt_llm_call_cloud(ctx, &ctx->history,
                                     resp.text, sizeof(resp.text));
        }
    }

    if (err != WLT_AI_OK) {
        resp.error = err;
        snprintf(resp.text, sizeof(resp.text),
                 "[Error: LLM call failed (%s)]",
                 wlt_ai_strerror(err));
        return resp;
    }

    /* Check for embedded tool calls in the LLM output */
    wlt_parse_tool_call(resp.text, &resp);

    /* If a tool was requested, execute it and feed result back */
    if (resp.has_tool_call) {
        wlt_ai_tool_result_t result;
        memset(&result, 0, sizeof(result));

        result = wlt_ai_system_control(ctx, &resp.tool_call);

        /* Append the assistant's tool-call message */
        wlt_memory_add_message(&ctx->history, WLT_AI_ROLE_ASSISTANT,
                               resp.text);

        /* Append the tool result as a system message */
        wlt_memory_add_message(&ctx->history, WLT_AI_ROLE_SYSTEM,
                               result.output);

        /* Second LLM call to produce a natural-language summary */
        char final_resp[WLT_AI_MAX_RESPONSE];
        wlt_ai_err_t err2;

        if (ctx->config.mode == WLT_AI_MODE_CLOUD) {
            err2 = wlt_llm_call_cloud(ctx, &ctx->history,
                                      final_resp, sizeof(final_resp));
        } else {
            err2 = wlt_llm_call_local(ctx, &ctx->history,
                                      final_resp, sizeof(final_resp));
        }

        if (err2 == WLT_AI_OK) {
            strncpy(resp.text, final_resp, sizeof(resp.text) - 1);
            resp.text[sizeof(resp.text) - 1] = '\0';
        }
    }

    /* Record the final assistant response */
    wlt_memory_add_message(&ctx->history, WLT_AI_ROLE_ASSISTANT,
                           resp.text);
    resp.success = true;

    /* Speak the response if voice output is enabled */
    if (ctx->config.voice_output) {
        wlt_tts_speak(ctx, resp.text);
    }

    return resp;
}

/* ------------------------------------------------------------------ */
/* Voice input / output                                               */
/* ------------------------------------------------------------------ */

wlt_ai_response_t
wlt_ai_voice_input(wlt_ai_ctx_t *ctx, const uint8_t *audio,
                   size_t audio_len)
{
    wlt_ai_response_t resp;
    char transcript[WLT_AI_MAX_MSG_LEN];
    wlt_ai_err_t err;

    memset(&resp, 0, sizeof(resp));

    if (ctx == NULL || audio == NULL || audio_len == 0) {
        resp.error = WLT_AI_ERR_INVALID;
        return resp;
    }

    /* Step 1: Speech-to-Text */
    err = wlt_stt_transcribe(ctx, audio, audio_len,
                             transcript, sizeof(transcript));
    if (err != WLT_AI_OK) {
        resp.error = err;
        snprintf(resp.text, sizeof(resp.text),
                 "[Error: STT failed (%s)]", wlt_ai_strerror(err));
        return resp;
    }

    if (strlen(transcript) == 0) {
        resp.error = WLT_AI_ERR_MODEL;
        strncpy(resp.text, "[No speech detected]",
                sizeof(resp.text) - 1);
        return resp;
    }

    /* Step 2: Process the transcript as a text query */
    resp = wlt_ai_query(ctx, transcript);
    return resp;
}

wlt_ai_err_t
wlt_ai_voice_output(wlt_ai_ctx_t *ctx, const char *text)
{
    if (ctx == NULL || text == NULL)
        return WLT_AI_ERR_INVALID;
    return wlt_tts_speak(ctx, text);
}

/* ------------------------------------------------------------------ */
/* Tool dispatch                                                      */
/* ------------------------------------------------------------------ */

wlt_ai_tool_result_t
wlt_ai_system_control(wlt_ai_ctx_t *ctx,
                      const wlt_ai_tool_call_t *call)
{
    wlt_ai_tool_result_t result;
    memset(&result, 0, sizeof(result));

    if (ctx == NULL || call == NULL) {
        result.success = false;
        strncpy(result.output, "[Invalid tool call]",
                sizeof(result.output) - 1);
        return result;
    }

    switch (call->type) {
    case WLT_TOOL_OPEN_APP:
        result = wlt_tool_open_app(call->args);
        break;
    case WLT_TOOL_CHANGE_SETTING:
        result = wlt_tool_change_setting(call->args);
        break;
    case WLT_TOOL_RUN_COMMAND:
        result = wlt_tool_run_command(call->args);
        break;
    case WLT_TOOL_FILE_SEARCH:
        result = wlt_tool_file_search(call->args);
        break;
    case WLT_TOOL_SCREEN_ANALYZE: {
        char vlm_result[WLT_AI_MAX_TOOL_RESULT];
        wlt_ai_err_t err = wlt_vision_describe(&ctx->config,
                                               vlm_result,
                                               sizeof(vlm_result));
        if (err == WLT_AI_OK) {
            result.success = true;
            strncpy(result.output, vlm_result,
                    sizeof(result.output) - 1);
        } else {
            result.success = false;
            snprintf(result.output, sizeof(result.output),
                     "[Screen analysis failed: %s]",
                     wlt_ai_strerror(err));
        }
        break;
    }
    default:
        result.success = false;
        strncpy(result.output, "[Unknown tool type]",
                sizeof(result.output) - 1);
        break;
    }

    return result;
}

wlt_ai_tool_result_t
wlt_ai_file_search(wlt_ai_ctx_t *ctx, const char *query)
{
    wlt_ai_tool_call_t call;
    memset(&call, 0, sizeof(call));
    call.type = WLT_TOOL_FILE_SEARCH;
    strncpy(call.args, query ? query : "", sizeof(call.args) - 1);
    return wlt_ai_system_control(ctx, &call);
}

/* ------------------------------------------------------------------ */
/* History access                                                     */
/* ------------------------------------------------------------------ */

const wlt_ai_history_t *
wlt_ai_get_history(wlt_ai_ctx_t *ctx)
{
    if (ctx == NULL)
        return NULL;
    return &ctx->history;
}

void
wlt_ai_clear_history(wlt_ai_ctx_t *ctx)
{
    if (ctx == NULL)
        return;
    memset(&ctx->history, 0, sizeof(ctx->history));
    wlt_memory_clear();
}

/* ------------------------------------------------------------------ */
/* LLM integration â local (llama.cpp server)                         */
/* ------------------------------------------------------------------ */

static wlt_ai_err_t
wlt_llm_call_local(wlt_ai_ctx_t *ctx, const wlt_ai_history_t *history,
                   char *response, size_t resp_len)
{
    char json_body[8192];
    struct curl_slist *headers = NULL;
    wlt_buf_t resp_buf = {0};
    long http_code = 0;
    CURLcode rc;

    if (ctx->config.local_llm_endpoint[0] == '\0') {
        fprintf(stderr, "wlt-ai: no local LLM endpoint configured\n");
        return WLT_AI_ERR_CONFIG;
    }

    /* Build the OpenAI-compatible chat completions JSON */
    wlt_build_chat_json(history, NULL, json_body, sizeof(json_body));

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_reset(ctx->curl);
    curl_easy_setopt(ctx->curl, CURLOPT_URL,
                     ctx->config.local_llm_endpoint);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION,
                     wlt_curl_write_cb);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, &resp_buf);
    curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT, 30L);

    rc = curl_easy_perform(ctx->curl);
    curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        fprintf(stderr, "wlt-ai: local LLM curl error: %s\n",
                curl_easy_strerror(rc));
        wlt_buf_free(&resp_buf);
        return WLT_AI_ERR_NETWORK;
    }

    if (http_code != 200 || resp_buf.data == NULL) {
        fprintf(stderr, "wlt-ai: local LLM HTTP %ld\n", http_code);
        wlt_buf_free(&resp_buf);
        return WLT_AI_ERR_MODEL;
    }

    /* Extract the assistant's message from JSON response */
    if (wlt_extract_json_field(resp_buf.data, "content",
                               response, resp_len) == NULL) {
        /* Fallback: use raw response */
        strncpy(response, resp_buf.data, resp_len - 1);
        response[resp_len - 1] = '\0';
    }

    wlt_buf_free(&resp_buf);
    return WLT_AI_OK;
}

/* ------------------------------------------------------------------ */
/* LLM integration â cloud (Gemini/Claude/GLM)                        */
/* ------------------------------------------------------------------ */

static wlt_ai_err_t
wlt_llm_call_cloud(wlt_ai_ctx_t *ctx, const wlt_ai_history_t *history,
                   char *response, size_t resp_len)
{
    char json_body[8192];
    char url[WLT_AI_ENDPOINT_LEN + WLT_AI_API_KEY_LEN + 32];
    struct curl_slist *headers = NULL;
    char auth_header[512];
    wlt_buf_t resp_buf = {0};
    long http_code = 0;
    CURLcode rc;

    if (ctx->config.cloud_provider == WLT_AI_CLOUD_NONE ||
        ctx->config.cloud_api_key[0] == '\0') {
        fprintf(stderr, "wlt-ai: no cloud provider configured\n");
        return WLT_AI_ERR_CONFIG;
    }

    wlt_build_chat_json(history, NULL, json_body, sizeof(json_body));

    /* Build auth header and URL based on provider */
    switch (ctx->config.cloud_provider) {
    case WLT_AI_CLOUD_GEMINI:
        snprintf(url, sizeof(url),
                 "https://generativelanguage.googleapis.com/"
                 "v1beta/models/gemini-pro:generateContent?key=%s",
                 ctx->config.cloud_api_key);
        break;
    case WLT_AI_CLOUD_CLAUDE:
        snprintf(url, sizeof(url), "%s/v1/messages",
                 ctx->config.cloud_endpoint[0]
                     ? ctx->config.cloud_endpoint
                     : "https://api.anthropic.com");
        snprintf(auth_header, sizeof(auth_header),
                 "x-api-key: %s", ctx->config.cloud_api_key);
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers,
                                    "anthropic-version: 2023-06-01");
        break;
    case WLT_AI_CLOUD_GLM:
        snprintf(url, sizeof(url), "%s/v1/chat/completions",
                 ctx->config.cloud_endpoint[0]
                     ? ctx->config.cloud_endpoint
                     : "https://open.bigmodel.cn/api/paas");
        snprintf(auth_header, sizeof(auth_header),
                 "Authorization: Bearer %s",
                 ctx->config.cloud_api_key);
        headers = curl_slist_append(headers, auth_header);
        break;
    default:
        return WLT_AI_ERR_CONFIG;
    }

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_reset(ctx->curl);
    curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION,
                     wlt_curl_write_cb);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, &resp_buf);
    curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT, 30L);

    rc = curl_easy_perform(ctx->curl);
    curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        fprintf(stderr, "wlt-ai: cloud LLM curl error: %s\n",
                curl_easy_strerror(rc));
        wlt_buf_free(&resp_buf);
        return WLT_AI_ERR_NETWORK;
    }

    if (http_code != 200 || resp_buf.data == NULL) {
        fprintf(stderr, "wlt-ai: cloud LLM HTTP %ld\n", http_code);
        wlt_buf_free(&resp_buf);
        return WLT_AI_ERR_MODEL;
    }

    /* Try to extract content from various JSON schemas */
    if (wlt_extract_json_field(resp_buf.data, "content",
                               response, resp_len) == NULL &&
        wlt_extract_json_field(resp_buf.data, "text",
                               response, resp_len) == NULL) {
        strncpy(response, resp_buf.data, resp_len - 1);
        response[resp_len - 1] = '\0';
    }

    wlt_buf_free(&resp_buf);
    return WLT_AI_OK;
}

/* ------------------------------------------------------------------ */
/* STT â local Whisper server                                         */
/* ------------------------------------------------------------------ */

static wlt_ai_err_t
wlt_stt_transcribe(wlt_ai_ctx_t *ctx, const uint8_t *audio,
                   size_t audio_len, char *text, size_t text_len)
{
    char url[WLT_AI_ENDPOINT_LEN + 64];
    struct curl_slist *headers = NULL;
    wlt_buf_t resp_buf = {0};
    long http_code = 0;
    CURLcode rc;

    if (ctx->config.local_stt_endpoint[0] == '\0') {
        fprintf(stderr, "wlt-ai: no STT endpoint configured\n");
        return WLT_AI_ERR_CONFIG;
    }

    snprintf(url, sizeof(url), "%s/v1/audio/transcriptions",
             ctx->config.local_stt_endpoint);

    headers = curl_slist_append(headers,
                                "Content-Type: application/octet-stream");

    curl_easy_reset(ctx->curl);
    curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, (const char *)audio);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE,
                     (long)audio_len);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION,
                     wlt_curl_write_cb);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, &resp_buf);
    curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT, 15L);

    rc = curl_easy_perform(ctx->curl);
    curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK || http_code != 200 || resp_buf.data == NULL) {
        wlt_buf_free(&resp_buf);
        return WLT_AI_ERR_MODEL;
    }

    if (wlt_extract_json_field(resp_buf.data, "text",
                               text, text_len) == NULL) {
        strncpy(text, resp_buf.data, text_len - 1);
        text[text_len - 1] = '\0';
    }

    wlt_buf_free(&resp_buf);
    return WLT_AI_OK;
}

/* ------------------------------------------------------------------ */
/* TTS â local Coqui TTS server                                       */
/* ------------------------------------------------------------------ */

static wlt_ai_err_t
wlt_tts_speak(wlt_ai_ctx_t *ctx, const char *text)
{
    char url[WLT_AI_ENDPOINT_LEN + 64];
    char json_body[WLT_AI_MAX_MSG_LEN + 64];
    struct curl_slist *headers = NULL;
    long http_code = 0;
    CURLcode rc;
    char escaped[WLT_AI_MAX_MSG_LEN];

    if (ctx->config.local_tts_endpoint[0] == '\0') {
        return WLT_AI_ERR_CONFIG;
    }

    wlt_json_escape(escaped, sizeof(escaped), text);
    snprintf(json_body, sizeof(json_body),
             "{\"text\":\"%s\"}", escaped);

    snprintf(url, sizeof(url), "%s/v1/tts", ctx->config.local_tts_endpoint);

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_reset(ctx->curl);
    curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT, 10L);

    rc = curl_easy_perform(ctx->curl);
    curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK || http_code != 200) {
        return WLT_AI_ERR_MODEL;
    }

    return WLT_AI_OK;
}

/* ------------------------------------------------------------------ */
/* Tool-call parsing                                                  */
/*                                                                     */
/* The LLM is prompted to embed tool calls in the format:             */
/*   [TOOL: open_app args="firefox"]                                  */
/*   [TOOL: change_setting args="volume 50"]                          */
/*   [TOOL: run_command args="ls -la"]                                */
/*   [TOOL: file_search args="report.pdf"]                            */
/*   [TOOL: screen_analyze args="what is on screen"]                  */
/* ------------------------------------------------------------------ */

static void
wlt_parse_tool_call(const char *llm_output, wlt_ai_response_t *resp)
{
    const char *marker = "[TOOL:";
    const char *p;
    const char *args_start;
    const char *args_end;
    char tool_name[64];
    size_t i;

    if (llm_output == NULL || resp == NULL)
        return;

    p = strstr(llm_output, marker);
    if (p == NULL)
        return;

    p += strlen(marker);

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t')
        p++;

    /* Extract tool name */
    i = 0;
    while (*p && *p != ' ' && *p != ']' && *p != '\n' && i < 63) {
        tool_name[i++] = *p++;
    }
    tool_name[i] = '\0';

    /* Map tool name to type */
    if (strcmp(tool_name, "open_app") == 0) {
        resp->tool_call.type = WLT_TOOL_OPEN_APP;
    } else if (strcmp(tool_name, "change_setting") == 0) {
        resp->tool_call.type = WLT_TOOL_CHANGE_SETTING;
    } else if (strcmp(tool_name, "run_command") == 0) {
        resp->tool_call.type = WLT_TOOL_RUN_COMMAND;
    } else if (strcmp(tool_name, "file_search") == 0) {
        resp->tool_call.type = WLT_TOOL_FILE_SEARCH;
    } else if (strcmp(tool_name, "screen_analyze") == 0) {
        resp->tool_call.type = WLT_TOOL_SCREEN_ANALYZE;
    } else {
        return; /* Unknown tool, not a tool call */
    }

    /* Extract args="..." */
    args_start = strstr(p, "args=\"");
    if (args_start != NULL) {
        args_start += 6; /* Skip 'args="' */
        args_end = strchr(args_start, '"');
        if (args_end != NULL) {
            size_t arg_len = (size_t)(args_end - args_start);
            if (arg_len >= sizeof(resp->tool_call.args))
                arg_len = sizeof(resp->tool_call.args) - 1;
            memcpy(resp->tool_call.args, args_start, arg_len);
            resp->tool_call.args[arg_len] = '\0';
        }
    } else {
        /* Fallback: everything after tool name until ] */
        while (*p == ' ' || *p == '\t')
            p++;
        args_end = strchr(p, ']');
        if (args_end == NULL)
            args_end = p + strlen(p);
        size_t arg_len = (size_t)(args_end - p);
        if (arg_len >= sizeof(resp->tool_call.args))
            arg_len = sizeof(resp->tool_call.args) - 1;
        memcpy(resp->tool_call.args, p, arg_len);
        resp->tool_call.args[arg_len] = '\0';
    }

    resp->has_tool_call = true;
}

/* ------------------------------------------------------------------ */
/* HTTP response buffer helpers                                       */
/* ------------------------------------------------------------------ */

static size_t
wlt_curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    wlt_buf_t *buf = (wlt_buf_t *)userdata;
    size_t total = size * nmemb;
    return wlt_buf_append(buf, (const char *)ptr, total) == 0
               ? total : 0;
}

static int
wlt_buf_append(wlt_buf_t *buf, const char *data, size_t len)
{
    size_t new_cap;
    char *new_data;

    if (buf->size + len + 1 > buf->capacity) {
        new_cap = buf->capacity;
        if (new_cap == 0)
            new_cap = 4096;
        while (new_cap < buf->size + len + 1)
            new_cap *= 2;
        new_data = realloc(buf->data, new_cap);
        if (new_data == NULL)
            return -1;
        buf->data = new_data;
        buf->capacity = new_cap;
    }

    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
    return 0;
}

static void
wlt_buf_free(wlt_buf_t *buf)
{
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}

/* ------------------------------------------------------------------ */
/* JSON helpers                                                       */
/* ------------------------------------------------------------------ */

static const char *
wlt_role_str(wlt_ai_role_t role)
{
    switch (role) {
    case WLT_AI_ROLE_SYSTEM:    return "system";
    case WLT_AI_ROLE_USER:      return "user";
    case WLT_AI_ROLE_ASSISTANT: return "assistant";
    default:                    return "user";
    }
}

static void
wlt_json_escape(char *out, size_t out_len, const char *in)
{
    size_t i = 0;

    while (*in && i < out_len - 2) {
        switch (*in) {
        case '"':  out[i++] = '\\'; out[i++] = '"'; break;
        case '\\': out[i++] = '\\'; out[i++] = '\\'; break;
        case '\n': out[i++] = '\\'; out[i++] = 'n'; break;
        case '\r': out[i++] = '\\'; out[i++] = 'r'; break;
        case '\t': out[i++] = '\\'; out[i++] = 't'; break;
        default:
            if ((unsigned char)*in < 0x20) {
                i += (size_t)snprintf(out + i, out_len - i,
                                      "\\u%04x", (unsigned)*in);
            } else {
                out[i++] = *in;
            }
            break;
        }
        in++;
    }
    out[i] = '\0';
}

static char *
wlt_extract_json_field(const char *json, const char *field,
                       char *out, size_t out_len)
{
    char pattern[128];
    const char *p;
    const char *start;
    const char *end;

    snprintf(pattern, sizeof(pattern), "\"%s\"", field);
    p = strstr(json, pattern);
    if (p == NULL)
        return NULL;

    p += strlen(pattern);

    /* Skip : and whitespace */
    while (*p == ':' || *p == ' ' || *p == '\t' || *p == '\n')
        p++;

    if (*p == '"') {
        /* String value */
        p++;
        start = p;
        end = p;
        while (*end) {
            if (*end == '\\' && *(end + 1)) {
                end += 2;
                continue;
            }
            if (*end == '"')
                break;
            end++;
        }
        size_t len = (size_t)(end - start);
        if (len >= out_len)
            len = out_len - 1;
        memcpy(out, start, len);
        out[len] = '\0';
    } else {
        /* Non-string: copy until , or } */
        start = p;
        end = p;
        while (*end && *end != ',' && *end != '}' && *end != '\n')
            end++;
        size_t len = (size_t)(end - start);
        if (len >= out_len)
            len = out_len - 1;
        memcpy(out, start, len);
        out[len] = '\0';
    }

    return out;
}

static void
wlt_build_chat_json(const wlt_ai_history_t *history,
                    const char *system_prompt,
                    char *json, size_t json_len)
{
    size_t offset = 0;
    size_t i;
    char escaped[WLT_AI_MAX_MSG_LEN * 2];

    offset += (size_t)snprintf(json + offset, json_len - offset,
        "{\"model\":\"%s\",\"messages\":[",
        "default");

    for (i = 0; i < history->count && offset < json_len - 256; i++) {
        const wlt_ai_message_t *msg = &history->messages[i];

        if (i > 0 && offset < json_len - 1)
            json[offset++] = ',';

        wlt_json_escape(escaped, sizeof(escaped), msg->content);
        offset += (size_t)snprintf(json + offset, json_len - offset,
            "{\"role\":\"%s\",\"content\":\"%s\"}",
            wlt_role_str(msg->role), escaped);
    }

    if (system_prompt && offset < json_len - 256) {
        wlt_json_escape(escaped, sizeof(escaped), system_prompt);
        offset += (size_t)snprintf(json + offset, json_len - offset,
            ",{\"role\":\"system\",\"content\":\"%s\"}", escaped);
    }

    if (offset < json_len - 16) {
        offset += (size_t)snprintf(json + offset, json_len - offset,
            "],\"temperature\":0.7,\"max_tokens\":512}");
    }
}
