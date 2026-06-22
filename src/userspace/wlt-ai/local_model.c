/**
 * @file local_model.c
 * @brief Local LLM integration — VibeThinker-3B / Llama 3 via llama.cpp
 * @module wlt-ai/local_model
 * @purpose Run inference on local models, no cloud needed
 */
#include "ai.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define WLT_LM_MAX_CONTEXT 4096
#define WLT_LM_MAX_TOKENS  1024

typedef struct {
    bool loaded;
    char model_path[512];
    char model_name[128];
    int n_ctx;          /* context window size */
    int n_threads;       /* CPU threads for inference */
    int n_gpu_layers;    /* GPU layers to offload (0 = CPU only) */
    float temperature;
    float top_p;
    int top_k;
    int repeat_penalty;
    /* In production: llama_context *ctx; */
    void *ctx;           /* Opaque llama.cpp context */
} wlt_lm_state_t;

static wlt_lm_state_t g_lm = {0};

wlt_ai_err_t
wlt_lm_load(const char *model_path, const char *model_name)
{
    if (!model_path) return WLT_AI_ERR_INVALID;

    /* In production:
     * llama_backend_init();
     * llama_model_params mparams = llama_model_default_params();
     * mparams.n_gpu_layers = g_lm.n_gpu_layers;
     * llama_model *model = llama_load_model_from_file(model_path, mparams);
     * llama_context_params cparams = llama_context_default_params();
     * cparams.n_ctx = g_lm.n_ctx;
     * cparams.n_threads = g_lm.n_threads;
     * g_lm.ctx = llama_new_context_with_model(model, cparams);
     */

    strncpy(g_lm.model_path, model_path, sizeof(g_lm.model_path) - 1);
    if (model_name)
        strncpy(g_lm.model_name, model_name, sizeof(g_lm.model_name) - 1);
    g_lm.n_ctx = WLT_LM_MAX_CONTEXT;
    g_lm.n_threads = 4;
    g_lm.n_gpu_layers = 0; /* CPU only — no GPU on target hardware */
    g_lm.temperature = 0.7;
    g_lm.top_p = 0.95;
    g_lm.top_k = 40;
    g_lm.repeat_penalty = 1.1;
    g_lm.loaded = true;
    g_lm.ctx = NULL; /* Will be set when llama.cpp is linked */

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_lm_infer(const char *prompt, char *response, size_t response_size)
{
    if (!prompt || !response) return WLT_AI_ERR_INVALID;
    if (!g_lm.loaded) return WLT_AI_ERR_MODEL;

    /* In production:
     * llama_token tokens[WLT_LM_MAX_CONTEXT];
     * int n_tokens = llama_tokenize(model, prompt, tokens, WLT_LM_MAX_CONTEXT, true);
     * llama_decode(ctx, llama_batch_get_one(tokens, n_tokens, 0, 0));
     * Sample next token, append to response, repeat until EOS or max_tokens
     */

    /* Placeholder: return a canned response (real inference needs llama.cpp linked) */
    snprintf(response, response_size,
             "[Local model %s] I received your request: \"%s\". "
             "Full local inference requires llama.cpp to be linked at build time. "
             "Model: %s at %s, ctx=%d, threads=%d, temp=%.1f",
             g_lm.model_name, prompt,
             g_lm.model_name, g_lm.model_path,
             g_lm.n_ctx, g_lm.n_threads, g_lm.temperature);

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_lm_set_params(float temperature, float top_p, int top_k, int threads)
{
    if (temperature > 0) g_lm.temperature = temperature;
    if (top_p > 0) g_lm.top_p = top_p;
    if (top_k > 0) g_lm.top_k = top_k;
    if (threads > 0) g_lm.n_threads = threads;
    return WLT_AI_OK;
}

bool
wlt_lm_is_loaded(void)
{
    return g_lm.loaded;
}

const char *
wlt_lm_get_model_name(void)
{
    return g_lm.loaded ? g_lm.model_name : "(none)";
}

void
wlt_lm_unload(void)
{
    /* In production: llama_free(g_lm.ctx); llama_backend_free(); */
    g_lm.loaded = false;
    g_lm.ctx = NULL;
}

/* Recommended model configurations for Kairos */
typedef struct {
    const char *name;
    const char *path;
    int ram_mb;        /* Approximate RAM at Q4_K_M */
    int n_ctx;
    bool tool_calling; /* Supports function/tool calling */
} wlt_lm_model_rec_t;

static const wlt_lm_model_rec_t recommended_models[] = {
    {"VibeThinker-3B",     "/opt/wltios/models/vibethinker-3b-q4_k_m.gguf",  2048, 4096, true},
    {"Qwen2.5-3B-Instruct","/opt/wltios/models/qwen2.5-3b-instruct-q4_k_m.gguf", 2048, 4096, true},
    {"Llama-3.2-3B",       "/opt/wltios/models/llama-3.2-3b-q4_k_m.gguf",    2048, 4096, false},
    {"Phi-3.5-mini",       "/opt/wltios/models/phi-3.5-mini-q4_k_m.gguf",    2560, 4096, false},
    {NULL, NULL, 0, 0, false}
};

const wlt_lm_model_rec_t *
wlt_lm_get_recommended_models(void)
{
    return recommended_models;
}
