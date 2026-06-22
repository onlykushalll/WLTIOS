/**
 * @file voice.c
 * @brief Voice I/O — Text-to-Speech (TTS) + Speech-to-Text (STT)
 * @module wlt-ai/voice
 * @purpose Kairos speaks responses (TTS) and listens to commands (STT)
 * @dependencies: none (uses espeak or Piper for TTS, whisper.cpp for STT)
 * @spec: docs/specs/wlt-ai.md
 */

#include "ai.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#define WLT_VOICE_MAX_TEXT   8192
#define WLT_VOICE_MAX_QUEUE  16

typedef enum {
    WLT_VOICE_TTS_IDLE,
    WLT_VOICE_TTS_SPEAKING,
    WLT_VOICE_TTS_QUEUED,
} wlt_voice_tts_state_t;

typedef struct {
    char text[WLT_VOICE_MAX_TEXT];
    bool is_warning;
} wlt_voice_msg_t;

typedef struct {
    bool tts_enabled;
    bool stt_enabled;
    wlt_voice_tts_state_t tts_state;
    wlt_voice_msg_t queue[WLT_VOICE_MAX_QUEUE];
    size_t queue_count;
    pthread_t tts_thread;
    bool running;
    char voice_name[64];
    int speed;      /* Words per minute */
    int pitch;      /* 0-99, default 50 */
    int volume;     /* 0-100, default 100 */
    void (*on_speech)(const char *text, void *user_data);
    void *stt_user_data;
} wlt_voice_state_t;

static wlt_voice_state_t g_voice = {0};

static void *
tts_thread_func(void *arg)
{
    (void)arg;
    while (g_voice.running) {
        if (g_voice.queue_count > 0) {
            wlt_voice_msg_t *msg = &g_voice.queue[0];
            g_voice.tts_state = WLT_VOICE_TTS_SPEAKING;

            /* In production: use Piper (neural TTS) or espeak (lightweight TTS)
             * Piper: piper_tts(msg->text, g_voice.voice_name, g_voice.speed)
             * espeak: espeak_Synth(msg->text, ...)
             */
            /* For now: log to stderr (no audio device in test env) */
            fprintf(stderr, "[Kairos TTS] %s\n", msg->text);

            /* Remove from queue */
            memmove(&g_voice.queue[0], &g_voice.queue[1],
                    (WLT_VOICE_MAX_QUEUE - 1) * sizeof(wlt_voice_msg_t));
            g_voice.queue_count--;
            g_voice.tts_state = WLT_VOICE_TTS_IDLE;
        }
        usleep(100000); /* 100ms poll */
    }
    return NULL;
}

wlt_ai_err_t
wlt_voice_init(void)
{
    memset(&g_voice, 0, sizeof(g_voice));
    g_voice.tts_enabled = true;
    g_voice.stt_enabled = true;
    g_voice.tts_state = WLT_VOICE_TTS_IDLE;
    strncpy(g_voice.voice_name, "en_US-lessac-medium", sizeof(g_voice.voice_name) - 1);
    g_voice.speed = 160;   /* words per minute */
    g_voice.pitch = 50;
    g_voice.volume = 100;
    g_voice.running = true;

    if (pthread_create(&g_voice.tts_thread, NULL, tts_thread_func, NULL) != 0) {
        g_voice.running = false;
        return WLT_AI_ERR_IO;
    }

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_voice_speak(const char *text)
{
    if (text == NULL)
        return WLT_AI_ERR_INVALID;

    if (!g_voice.tts_enabled)
        return WLT_AI_OK;

    if (g_voice.queue_count >= WLT_VOICE_MAX_QUEUE)
        return WLT_AI_ERR_NOMEM;

    wlt_voice_msg_t *msg = &g_voice.queue[g_voice.queue_count];
    strncpy(msg->text, text, WLT_VOICE_MAX_TEXT - 1);
    msg->text[WLT_VOICE_MAX_TEXT - 1] = '\0';
    msg->is_warning = false;
    g_voice.queue_count++;

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_voice_speak_warning(const char *text)
{
    if (text == NULL)
        return WLT_AI_ERR_INVALID;

    /* Warnings go to front of queue */
    if (g_voice.queue_count >= WLT_VOICE_MAX_QUEUE)
        return WLT_AI_ERR_NOMEM;

    memmove(&g_voice.queue[1], &g_voice.queue[0],
            g_voice.queue_count * sizeof(wlt_voice_msg_t));
    wlt_voice_msg_t *msg = &g_voice.queue[0];
    strncpy(msg->text, text, WLT_VOICE_MAX_TEXT - 1);
    msg->text[WLT_VOICE_MAX_TEXT - 1] = '\0';
    msg->is_warning = true;
    g_voice.queue_count++;

    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_voice_start_listening(void (*on_speech)(const char *text, void *user_data), void *user_data)
{
    if (on_speech == NULL)
        return WLT_AI_ERR_INVALID;

    g_voice.on_speech = on_speech;
    g_voice.stt_user_data = user_data;

    /* In production: start whisper.cpp continuous listening mode
     * whisper_context *ctx = whisper_init_from_file("base.en.bin");
     * Capture audio via ALSA, feed to whisper, call on_speech when text decoded
     */
    return WLT_AI_OK;
}

void
wlt_voice_stop_listening(void)
{
    g_voice.on_speech = NULL;
}

wlt_ai_err_t
wlt_voice_set_voice(const char *voice_name, int speed, int pitch, int volume)
{
    if (voice_name)
        strncpy(g_voice.voice_name, voice_name, sizeof(g_voice.voice_name) - 1);
    if (speed > 0)
        g_voice.speed = speed;
    if (pitch >= 0 && pitch <= 99)
        g_voice.pitch = pitch;
    if (volume >= 0 && volume <= 100)
        g_voice.volume = volume;
    return WLT_AI_OK;
}

bool
wlt_voice_is_speaking(void)
{
    return g_voice.tts_state == WLT_VOICE_TTS_SPEAKING;
}

void
wlt_voice_shutdown(void)
{
    g_voice.running = false;
    pthread_join(g_voice.tts_thread, NULL);
    memset(&g_voice, 0, sizeof(g_voice));
}
