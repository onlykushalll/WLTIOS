/**
 * @file hotword.c
 * @brief Wake word detection — "Wake up, Buddy!"
 * @module wlt-ai/hotword
 * @purpose Listen for the wake word via microphone, trigger Kairos activation
 * @dependencies: none (uses ALSA or PulseAudio simple API)
 * @spec: docs/specs/wlt-ai.md
 * @adr: docs/adr/08-kairos.md
 *
 * Implementation notes:
 * - Uses simple energy-based keyword spotting (no ML model needed for basic detection)
 * - For production: integrate with openWakeWord or Porcupine for robust detection
 * - Audio captured via ALSA (snd_pcm) — no PulseAudio dependency
 * - Runs in a background thread, calls callback when wake word detected
 */

#include "ai.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#define WLT_HOTWORD_SAMPLE_RATE 16000
#define WLT_HOTWORD_CHANNELS    1
#define WLT_HOTWORD_BUFFER_MS   100
#define WLT_HOTWORD_THRESHOLD   0.6  /* Confidence threshold */

typedef struct {
    bool running;
    pthread_t thread;
    void (*on_wake)(void *user_data);
    void *user_data;
    bool detected;
} wlt_hotword_state_t;

static wlt_hotword_state_t g_hotword = {0};

/* Simple energy-based detection (placeholder for real KWS model).
 * In production, replace with openWakeWord (TensorFlow Lite) or Porcupine. */
static double
compute_energy(const int16_t *samples, size_t count)
{
    if (count == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double s = (double)samples[i] / 32768.0;
        sum += s * s;
    }
    return sum / (double)count;
}

/* Check if the audio pattern matches "Wake up, Buddy!".
 * This is a simplified phonetic energy pattern matcher.
 * Real implementation would use a neural network model. */
static bool
detect_wake_word(const int16_t *samples, size_t count)
{
    if (count < WLT_HOTWORD_SAMPLE_RATE / 2) /* Need at least 0.5s of audio */
        return false;

    /* Split into 3 segments (Wake / up / Buddy) and check energy patterns */
    size_t seg = count / 3;
    double e1 = compute_energy(samples, seg);
    double e2 = compute_energy(samples + seg, seg);
    double e3 = compute_energy(samples + 2 * seg, seg);

    /* Pattern: medium-high-low-high (Wake-up-Buddy rhythm) */
    double avg = (e1 + e2 + e3) / 3.0;
    if (avg < 0.01) return false; /* Too quiet */

    /* Check pattern: first segment louder, second quieter, third loudest */
    if (e1 > avg * 0.7 && e3 > avg * 1.2 && e2 < avg * 1.1)
        return true;

    return false;
}

static void *
hotword_thread(void *arg)
{
    (void)arg;
    int16_t buffer[WLT_HOTWORD_SAMPLE_RATE * WLT_HOTWORD_BUFFER_MS / 1000];

    while (g_hotword.running) {
        /* In production: read from ALSA PCM device */
        /* snd_pcm_readi(pcm_handle, buffer, sizeof(buffer)/sizeof(buffer[0])); */
        /* For now: sleep (no audio device in test environment) */
        usleep(WLT_HOTWORD_BUFFER_MS * 1000);

        /* Check for wake word */
        if (detect_wake_word(buffer, sizeof(buffer) / sizeof(buffer[0]))) {
            g_hotword.detected = true;
            if (g_hotword.on_wake) {
                g_hotword.on_wake(g_hotword.user_data);
            }
        }
    }

    return NULL;
}

wlt_ai_err_t
wlt_hotword_start(void (*callback)(void *user_data), void *user_data)
{
    if (callback == NULL)
        return WLT_AI_ERR_INVALID;

    if (g_hotword.running)
        return WLT_AI_OK; /* Already running */

    g_hotword.on_wake = callback;
    g_hotword.user_data = user_data;
    g_hotword.running = true;
    g_hotword.detected = false;

    if (pthread_create(&g_hotword.thread, NULL, hotword_thread, NULL) != 0) {
        g_hotword.running = false;
        return WLT_AI_ERR_IO;
    }

    return WLT_AI_OK;
}

void
wlt_hotword_stop(void)
{
    if (!g_hotword.running)
        return;

    g_hotword.running = false;
    pthread_join(g_hotword.thread, NULL);
    memset(&g_hotword, 0, sizeof(g_hotword));
}

bool
wlt_hotword_was_detected(void)
{
    bool detected = g_hotword.detected;
    g_hotword.detected = false;
    return detected;
}
