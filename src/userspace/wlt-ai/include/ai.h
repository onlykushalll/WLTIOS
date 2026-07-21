/*
 * include/ai.h - WLT-AI Public API
 *
 * WLT-AI is the built-in AI assistant for WLTIOS. It can see the screen
 * (VLM), hear voice (STT), speak responses (TTS), control the OS, and
 * answer questions. Runs locally by default; cloud is opt-in.
 *
 * Spec: docs/specs/wlt-ai.md (v1.0)
 * Phase: 4 (Month 19-20)
 *
 * Copyright (c) WLTIOS Project. All rights reserved.
 */

#ifndef WLT_AI_H
#define WLT_AI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define WLT_AI_VERSION          "1.0.0"
#define WLT_AI_CONFIG_PATH      "/etc/wltios/wlt-ai.conf"
#define WLT_AI_MEMORY_PATH      "/var/lib/wlt-ai/memory.enc"
#define WLT_AI_SCREENSHOT_PATH  "/tmp/wlt-ai-screen.png"

#define WLT_AI_MAX_MSG_LEN      4096
#define WLT_AI_MAX_RESPONSE     8192
#define WLT_AI_MAX_HISTORY      100
#define WLT_AI_MAX_TOOL_ARGS    512
#define WLT_AI_MAX_TOOL_RESULT  2048
#define WLT_AI_HOTWORD_LEN      64
#define WLT_AI_API_KEY_LEN      256
#define WLT_AI_MODEL_LEN        128
#define WLT_AI_ENDPOINT_LEN     256
#define WLT_AI_APP_NAME_LEN     128
#define WLT_AI_SETTING_LEN      256
#define WLT_AI_CMD_LEN          1024
#define WLT_AI_FILE_PATH_LEN    512
#define WLT_AI_ENCRYPT_KEY_LEN  32

/* ------------------------------------------------------------------ */
/* Enumerations                                                       */
/* ------------------------------------------------------------------ */

/* AI operating mode */
typedef enum {
    WLT_AI_MODE_LOCAL = 0,      /* Local models only (default) */
    WLT_AI_MODE_CLOUD,          /* Cloud API only */
    WLT_AI_MODE_HYBRID          /* Local + cloud fallback */
} wlt_ai_mode_t;

/* Cloud provider */
typedef enum {
    WLT_AI_CLOUD_NONE = 0,
    WLT_AI_CLOUD_GEMINI,
    WLT_AI_CLOUD_CLAUDE,
    WLT_AI_CLOUD_GLM
} wlt_ai_cloud_t;

/* Memory persistence mode */
typedef enum {
    WLT_AI_MEMORY_AMNESIC = 0,  /* Forget on reboot */
    WLT_AI_MEMORY_PERSISTENT    /* Encrypted, survives reboot */
} wlt_ai_memory_t;

/* Conversation message role */
typedef enum {
    WLT_AI_ROLE_SYSTEM = 0,
    WLT_AI_ROLE_USER,
    WLT_AI_ROLE_ASSISTANT
} wlt_ai_role_t;

/* Tool call type */
typedef enum {
    WLT_TOOL_NONE = 0,
    WLT_TOOL_OPEN_APP,          /* Launch an application */
    WLT_TOOL_CHANGE_SETTING,    /* Modify a system setting */
    WLT_TOOL_RUN_COMMAND,       /* Execute a shell command */
    WLT_TOOL_FILE_SEARCH,       /* Search for files */
    WLT_TOOL_SCREEN_ANALYZE     /* Capture + analyze screen */
} wlt_tool_type_t;

/* Error codes */
typedef enum {
    WLT_AI_OK = 0,
    WLT_AI_ERR_INVALID,
    WLT_AI_ERR_CONFIG,
    WLT_AI_ERR_NETWORK,
    WLT_AI_ERR_MEMORY,
    WLT_AI_ERR_MODEL,
    WLT_AI_ERR_TOOL,
    WLT_AI_ERR_ENCRYPT,
    WLT_AI_ERR_IO,
    WLT_AI_ERR_NOMEM
} wlt_ai_err_t;

/* ------------------------------------------------------------------ */
/* Data structures                                                    */
/* ------------------------------------------------------------------ */

/* Full configuration loaded from wlt-ai.conf */
typedef struct {
    bool                enabled;
    wlt_ai_mode_t       mode;
    char                hotword[WLT_AI_HOTWORD_LEN];
    bool                voice_output;
    wlt_ai_memory_t     memory_mode;

    /* Local model names */
    char                vlm_model[WLT_AI_MODEL_LEN];
    char                llm_model[WLT_AI_MODEL_LEN];
    char                stt_model[WLT_AI_MODEL_LEN];
    char                tts_model[WLT_AI_MODEL_LEN];

    /* Local model server endpoints (llama.cpp / whisper / coqui) */
    char                local_llm_endpoint[WLT_AI_ENDPOINT_LEN];
    char                local_vlm_endpoint[WLT_AI_ENDPOINT_LEN];
    char                local_stt_endpoint[WLT_AI_ENDPOINT_LEN];
    char                local_tts_endpoint[WLT_AI_ENDPOINT_LEN];

    /* Cloud configuration */
    wlt_ai_cloud_t      cloud_provider;
    char                cloud_api_key[WLT_AI_API_KEY_LEN];
    char                cloud_endpoint[WLT_AI_ENDPOINT_LEN];

    /* Memory encryption key (derived, not stored in plaintext) */
    uint8_t             encrypt_key[WLT_AI_ENCRYPT_KEY_LEN];
} wlt_ai_config_t;

/* Single conversation message */
typedef struct {
    wlt_ai_role_t       role;
    char                content[WLT_AI_MAX_MSG_LEN];
    time_t              timestamp;
} wlt_ai_message_t;

/* Conversation history (ring buffer) */
typedef struct {
    wlt_ai_message_t    messages[WLT_AI_MAX_HISTORY];
    size_t              count;
    size_t              head;           /* Next write slot */
} wlt_ai_history_t;

/* Parsed tool call extracted from LLM output */
typedef struct {
    wlt_tool_type_t     type;
    char                args[WLT_AI_MAX_TOOL_ARGS];
} wlt_ai_tool_call_t;

/* Result of executing a tool */
typedef struct {
    char                output[WLT_AI_MAX_TOOL_RESULT];
    bool                success;
} wlt_ai_tool_result_t;

/* AI response to a query */
typedef struct {
    char                text[WLT_AI_MAX_RESPONSE];
    wlt_ai_tool_call_t  tool_call;
    bool                has_tool_call;
    bool                success;
    wlt_ai_err_t        error;
} wlt_ai_response_t;

/* Main AI engine context (opaque to callers) */
typedef struct wlt_ai_ctx wlt_ai_ctx_t;

/* ------------------------------------------------------------------ */
/* ai.c â Main AI engine                                              */
/* ------------------------------------------------------------------ */

/* Initialize the AI engine. Returns NULL on failure. */
wlt_ai_ctx_t *wlt_ai_init(const wlt_ai_config_t *cfg);

/* Shut down and free all resources. */
void wlt_ai_shutdown(wlt_ai_ctx_t *ctx);

/* Process a text query and return a response. */
wlt_ai_response_t wlt_ai_query(wlt_ai_ctx_t *ctx, const char *query);

/* Process voice input: STT -> LLM -> (optional TTS). */
wlt_ai_response_t wlt_ai_voice_input(wlt_ai_ctx_t *ctx,
                                     const uint8_t *audio,
                                     size_t audio_len);

/* Speak text aloud via TTS. Returns WLT_AI_OK on success. */
wlt_ai_err_t wlt_ai_voice_output(wlt_ai_ctx_t *ctx, const char *text);

/* Dispatch a tool call and return the result. */
wlt_ai_tool_result_t wlt_ai_system_control(wlt_ai_ctx_t *ctx,
                                           const wlt_ai_tool_call_t *call);

/* Search for files matching a natural-language query. */
wlt_ai_tool_result_t wlt_ai_file_search(wlt_ai_ctx_t *ctx,
                                        const char *query);

/* Get the conversation history. */
const wlt_ai_history_t *wlt_ai_get_history(wlt_ai_ctx_t *ctx);

/* Clear the conversation history. */
void wlt_ai_clear_history(wlt_ai_ctx_t *ctx);

/* ------------------------------------------------------------------ */
/* tools.c â System control tools                                     */
/* ------------------------------------------------------------------ */

wlt_ai_tool_result_t wlt_tool_open_app(const char *app_name);
wlt_ai_tool_result_t wlt_tool_change_setting(const char *setting);
wlt_ai_tool_result_t wlt_tool_run_command(const char *command);
wlt_ai_tool_result_t wlt_tool_file_search(const char *query);

/* ------------------------------------------------------------------ */
/* memory.c â Encrypted conversation history                          */
/* ------------------------------------------------------------------ */

/* Initialize the memory subsystem with encryption key. */
wlt_ai_err_t wlt_memory_init(const char *path,
                             const uint8_t *key,
                             size_t key_len);

/* Save conversation history to disk (encrypted). */
wlt_ai_err_t wlt_memory_save(const wlt_ai_history_t *history);

/* Load conversation history from disk (decrypt). */
wlt_ai_err_t wlt_memory_load(wlt_ai_history_t *history);

/* Clear all stored memory (amnesic mode). */
wlt_ai_err_t wlt_memory_clear(void);

/* Add a message to the history ring buffer. */
void wlt_memory_add_message(wlt_ai_history_t *history,
                            wlt_ai_role_t role,
                            const char *content);

/* ------------------------------------------------------------------ */
/* vision.c â Screen capture + VLM integration                        */
/* ------------------------------------------------------------------ */

/* Capture the current screen to a PNG file. */
wlt_ai_err_t wlt_vision_capture(const char *output_path);

/* Analyze a screenshot with the VLM and return a text description. */
wlt_ai_err_t wlt_vision_analyze(const char *image_path,
                                const char *question,
                                const wlt_ai_config_t *cfg,
                                char *result,
                                size_t result_len);

/* Convenience: capture + analyze in one call. */
wlt_ai_err_t wlt_vision_describe(const wlt_ai_config_t *cfg,
                                 char *result,
                                 size_t result_len);

/* ------------------------------------------------------------------ */
/* config.c â Configuration parsing                                   */
/* ------------------------------------------------------------------ */

/* Load configuration from an INI file. Returns WLT_AI_OK on success. */
wlt_ai_err_t wlt_config_load(const char *path, wlt_ai_config_t *cfg);

/* Free resources associated with a config. */
void wlt_config_free(wlt_ai_config_t *cfg);

/* Get a human-readable string for an error code. */
const char *wlt_ai_strerror(wlt_ai_err_t err);

/* Get a string for a tool type. */
const char *wlt_tool_type_str(wlt_tool_type_t type);

/* anonymity_coach.c */
wlt_ai_err_t wlt_ai_anon_update_status(bool tor_ok, bool dns_tor, bool kill_switch, bool mac_rand, bool webrtc_ok, int circuits, const char *exit_country, const char *exit_ip);
wlt_ai_err_t wlt_ai_anon_check(char *warnings, size_t warn_size);
bool wlt_ai_anon_is_safe(void);

/* privacy_analysis.c */
wlt_ai_err_t wlt_ai_privacy_analyze(char *report, size_t report_size);
wlt_ai_err_t wlt_ai_privacy_register_app(const char *name, bool network, bool direct_net, bool files, bool camera, bool mic, bool background);
void wlt_ai_privacy_reset(void);

/* proactive_protection.c */
typedef enum {
    WLT_PP_EVENT_PORT_SCAN = 0,
    WLT_PP_EVENT_BRUTE_FORCE,
    WLT_PP_EVENT_SUSPICIOUS_PROCESS,
    WLT_PP_EVENT_DISK_ENCRYPTION_OFF,
    WLT_PP_EVENT_TOR_DISCONNECTED,
    WLT_PP_EVENT_NEW_USB_DEVICE,
    WLT_PP_EVENT_USER_IDLE,
    WLT_PP_EVENT_SCREEN_LOCK_OFF,
} wlt_pp_event_type_t;
wlt_ai_err_t wlt_pp_init(void (*on_alert)(wlt_pp_event_type_t, const char *, void *), void *user_data);
wlt_ai_err_t wlt_pp_report_event(wlt_pp_event_type_t type, const char *detail);
wlt_ai_err_t wlt_pp_user_activity(void);
wlt_ai_err_t wlt_pp_check_idle(void);
wlt_ai_err_t wlt_pp_get_report(char *report, size_t report_size);
void wlt_pp_resolve_event(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* WLT_AI_H */
