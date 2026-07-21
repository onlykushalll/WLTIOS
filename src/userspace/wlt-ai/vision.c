/*
 * vision.c - WLT-AI Screen Capture + VLM Integration
 *
 * Provides screen understanding capabilities. Captures the current
 * screen via the WLT-Compositor (or framebuffer fallback), encodes it
 * as PNG, and sends it to the Vision Language Model for analysis.
 *
 * The VLM is queried via an OpenAI-compatible API (llama.cpp server
 * with vision model, or cloud multimodal endpoint). The image is
 * base64-encoded and embedded in the request JSON.
 *
 * Performance target: < 2 seconds per frame analysis.
 *
 * Copyright (c) WLTIOS Project. All rights reserved.
 */

#include "ai.h"
#include "wlt_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <errno.h>

#include <curl/curl.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define WLT_FB_DEVICE        "/dev/fb0"
#define WLT_GRIM_CMD         "grim 2>/dev/null"           /* Wayland */
#define WLT_SCROT_CMD        "scrot 2>/dev/null"           /* X11 */
#define WLT_IMPORT_CMD       "import -window root 2>/dev/null" /* ImageMagick */

#define WLT_PNG_HEADER_LEN   8

/* Base64 encoding table */
static const char wlt_b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* ------------------------------------------------------------------ */
/* Internal types                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    char    *data;
    size_t   size;
    size_t   capacity;
} wlt_vbuf_t;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static wlt_ai_err_t wlt_capture_wayland(const char *path);
static wlt_ai_err_t wlt_capture_x11(const char *path);
static wlt_ai_err_t wlt_capture_framebuffer(const char *path);

static char *wlt_base64_encode(const uint8_t *data, size_t len);
static size_t wlt_base64_encoded_len(size_t len);

static size_t wlt_vbuf_write_cb(void *ptr, size_t size, size_t nmemb,
                                void *userdata);
static int wlt_vbuf_append(wlt_vbuf_t *buf, const char *data, size_t len);
static void wlt_vbuf_free(wlt_vbuf_t *buf);

static char *wlt_json_escape_str(const char *in, char *out, size_t out_len);
static const char *wlt_extract_json_text(const char *json,
                                         char *out, size_t out_len);

/* ------------------------------------------------------------------ */
/* Screen capture                                                     */
/* ------------------------------------------------------------------ */

wlt_ai_err_t
wlt_vision_capture(const char *output_path)
{
    wlt_ai_err_t err;

    if (output_path == NULL || output_path[0] == '\0')
        return WLT_AI_ERR_INVALID;

    /* Try Wayland (grim) first â WLTIOS uses Wayland */
    err = wlt_capture_wayland(output_path);
    if (err == WLT_AI_OK)
        return WLT_AI_OK;

    /* Fall back to X11 (scrot / import) */
    err = wlt_capture_x11(output_path);
    if (err == WLT_AI_OK)
        return WLT_AI_OK;

    /* Last resort: direct framebuffer read */
    err = wlt_capture_framebuffer(output_path);
    if (err == WLT_AI_OK)
        return WLT_AI_OK;

    fprintf(stderr, "wlt-ai: all screen capture methods failed\n");
    return WLT_AI_ERR_IO;
}

static wlt_ai_err_t
wlt_capture_wayland(const char *path)
{
    char cmd[WLT_AI_FILE_PATH_LEN + 64];
    int rc;

    snprintf(cmd, sizeof(cmd), "grim '%s' 2>/dev/null", path);
    rc = system(cmd);
    if (rc == 0 && access(path, R_OK) == 0)
        return WLT_AI_OK;
    return WLT_AI_ERR_IO;
}

static wlt_ai_err_t
wlt_capture_x11(const char *path)
{
    char cmd[WLT_AI_FILE_PATH_LEN + 64];
    int rc;

    /* Try scrot first */
    snprintf(cmd, sizeof(cmd), "scrot '%s' 2>/dev/null", path);
    rc = system(cmd);
    if (rc == 0 && access(path, R_OK) == 0)
        return WLT_AI_OK;

    /* Try ImageMagick import */
    snprintf(cmd, sizeof(cmd),
             "import -window root '%s' 2>/dev/null", path);
    rc = system(cmd);
    if (rc == 0 && access(path, R_OK) == 0)
        return WLT_AI_OK;

    return WLT_AI_ERR_IO;
}

static wlt_ai_err_t
wlt_capture_framebuffer(const char *path)
{
    int fb_fd;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    uint8_t *fb_mem = NULL;
    size_t fb_size;
    FILE *png_file = NULL;
    wlt_ai_err_t ret = WLT_AI_ERR_IO;

    fb_fd = open(WLT_FB_DEVICE, O_RDONLY);
    if (fb_fd < 0)
        return WLT_AI_ERR_IO;

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0)
        goto cleanup;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0)
        goto cleanup;

    fb_size = (size_t)finfo.smem_len;
    if (fb_size == 0)
        fb_size = (size_t)vinfo.yres_virtual * finfo.line_length;

    fb_mem = malloc(fb_size);
    if (fb_mem == NULL) {
        ret = WLT_AI_ERR_NOMEM;
        goto cleanup;
    }

    if (read(fb_fd, fb_mem, fb_size) != (ssize_t)fb_size)
        goto cleanup;

    /* Write raw framebuffer as a simple PPM (portable pixmap).
     * The VLM endpoint or a conversion step can handle PPM->PNG.
     * For production, use libpng here. */
    {
        char ppm_path[WLT_AI_FILE_PATH_LEN];
        size_t x, y;
        uint8_t *pixel;
        size_t bpp = vinfo.bits_per_pixel / 8;

        snprintf(ppm_path, sizeof(ppm_path), "%s.ppm", path);
        png_file = fopen(ppm_path, "wb");
        if (png_file == NULL)
            goto cleanup;

        fprintf(png_file, "P6\n%u %u\n255\n",
                vinfo.xres, vinfo.yres);

        for (y = 0; y < vinfo.yres; y++) {
            for (x = 0; x < vinfo.xres; x++) {
                size_t offset = y * finfo.line_length + x * bpp;
                if (offset + bpp > fb_size)
                    break;
                pixel = fb_mem + offset;

                /* Assume BGRA/XRGB â convert to RGB */
                uint8_t r, g, b;
                if (bpp >= 4) {
                    b = pixel[0]; g = pixel[1]; r = pixel[2];
                } else if (bpp == 3) {
                    r = pixel[0]; g = pixel[1]; b = pixel[2];
                } else {
                    r = g = b = pixel[0];
                }
                fputc(r, png_file);
                fputc(g, png_file);
                fputc(b, png_file);
            }
        }
        fclose(png_file);
        png_file = NULL;

        /* Rename .ppm to target path (since we can't do real PNG here) */
        rename(ppm_path, path);
    }

    ret = WLT_AI_OK;

cleanup:
    if (png_file)
        fclose(png_file);
    if (fb_mem)
        free(fb_mem);
    close(fb_fd);
    return ret;
}

/* ------------------------------------------------------------------ */
/* Base64 encoding                                                    */
/* ------------------------------------------------------------------ */

static size_t
wlt_base64_encoded_len(size_t len)
{
    return ((len + 2) / 3) * 4 + 1;
}

static char *
wlt_base64_encode(const uint8_t *data, size_t len)
{
    char *out;
    size_t out_len;
    size_t i, j;
    uint32_t triple;

    out_len = wlt_base64_encoded_len(len);
    out = malloc(out_len);
    if (out == NULL)
        return NULL;

    for (i = 0, j = 0; i < len;) {
        triple = 0;
        triple |= (uint32_t)(i < len ? data[i++] : 0) << 16;
        triple |= (uint32_t)(i < len ? data[i++] : 0) << 8;
        triple |= (uint32_t)(i < len ? data[i++] : 0);

        out[j++] = wlt_b64_table[(triple >> 18) & 0x3F];
        out[j++] = wlt_b64_table[(triple >> 12) & 0x3F];
        out[j++] = (i > len + 1) ? '=' :
                     wlt_b64_table[(triple >> 6) & 0x3F];
        out[j++] = (i > len) ? '=' :
                     wlt_b64_table[triple & 0x3F];
    }
    out[j] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* VLM analysis                                                       */
/* ------------------------------------------------------------------ */

static size_t
wlt_vbuf_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    wlt_vbuf_t *buf = (wlt_vbuf_t *)userdata;
    size_t total = size * nmemb;
    return wlt_vbuf_append(buf, (const char *)ptr, total) == 0
               ? total : 0;
}

static int
wlt_vbuf_append(wlt_vbuf_t *buf, const char *data, size_t len)
{
    size_t new_cap;
    char *new_data;

    if (buf->size + len + 1 > buf->capacity) {
        new_cap = buf->capacity ? buf->capacity : 8192;
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
wlt_vbuf_free(wlt_vbuf_t *buf)
{
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}

static char *
wlt_json_escape_str(const char *in, char *out, size_t out_len)
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
        }
        in++;
    }
    out[i] = '\0';
    return out;
}

static const char *
wlt_extract_json_text(const char *json, char *out, size_t out_len)
{
    const char *p;
    const char *start, *end;
    const char *patterns[] = { "\"content\"", "\"text\"", "\"description\"" };

    for (size_t i = 0; i < 3; i++) {
        p = strstr(json, patterns[i]);
        if (p == NULL)
            continue;
        p += strlen(patterns[i]);
        while (*p == ':' || *p == ' ' || *p == '\t' || *p == '\n')
            p++;
        if (*p != '"')
            continue;
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
        return out;
    }
    return NULL;
}

wlt_ai_err_t
wlt_vision_analyze(const char *image_path, const char *question,
                   const wlt_ai_config_t *cfg,
                   char *result, size_t result_len)
{
    FILE *img_file;
    uint8_t *img_data = NULL;
    char *b64_data = NULL;
    char *json_body = NULL;
    char *url = NULL;
    char resp_buf[32768] = {0};
    struct stat st;
    int http_code = 0;
    wlt_ai_err_t ret = WLT_AI_ERR_MODEL;
    char escaped_question[WLT_AI_MAX_MSG_LEN * 2];

    if (image_path == NULL || question == NULL || cfg == NULL ||
        result == NULL || result_len == 0)
        return WLT_AI_ERR_INVALID;

    /* Read image file */
    if (stat(image_path, &st) != 0) {
        fprintf(stderr, "wlt-ai: cannot stat %s: %s\n",
                image_path, strerror(errno));
        return WLT_AI_ERR_IO;
    }

    img_file = fopen(image_path, "rb");
    if (img_file == NULL)
        return WLT_AI_ERR_IO;

    img_data = malloc(st.st_size);
    if (img_data == NULL) {
        ret = WLT_AI_ERR_NOMEM;
        goto cleanup;
    }

    if (fread(img_data, 1, st.st_size, img_file) != (size_t)st.st_size)
        goto cleanup;
    fclose(img_file);
    img_file = NULL;

    /* Base64-encode the image */
    b64_data = wlt_base64_encode(img_data, st.st_size);
    if (b64_data == NULL) {
        ret = WLT_AI_ERR_NOMEM;
        goto cleanup;
    }

    /* Build the OpenAI-compatible vision API request JSON */
    wlt_json_escape_str(question, escaped_question, sizeof(escaped_question));

    {
        size_t body_len = strlen(b64_data) + strlen(escaped_question) + 512;
        json_body = malloc(body_len);
        if (json_body == NULL) {
            ret = WLT_AI_ERR_NOMEM;
            goto cleanup;
        }

        snprintf(json_body, body_len,
            "{\"model\":\"%s\",\"messages\":[{"
            "\"role\":\"user\",\"content\":["
            "{\"type\":\"text\",\"text\":\"%s\"},"
            "{\"type\":\"image_url\",\"image_url\":{"
            "\"url\":\"data:image/png;base64,%s\"}}"
            "]}],\"max_tokens\":512}",
            cfg->vlm_model, escaped_question, b64_data);
    }

    /* Determine endpoint URL */
    url = malloc(WLT_AI_ENDPOINT_LEN + WLT_AI_API_KEY_LEN + 64);
    if (url == NULL) {
        ret = WLT_AI_ERR_NOMEM;
        goto cleanup;
    }

    if (cfg->mode == WLT_AI_MODE_CLOUD) {
        switch (cfg->cloud_provider) {
        case WLT_AI_CLOUD_GEMINI:
            snprintf(url, WLT_AI_ENDPOINT_LEN + WLT_AI_API_KEY_LEN + 64,
                "https://generativelanguage.googleapis.com/"
                "v1beta/models/gemini-pro-vision:generateContent?key=%s",
                cfg->cloud_api_key);
            break;
        case WLT_AI_CLOUD_CLAUDE:
            snprintf(url, WLT_AI_ENDPOINT_LEN + WLT_AI_API_KEY_LEN + 64,
                "%s/v1/messages",
                cfg->cloud_endpoint[0] ? cfg->cloud_endpoint
                                       : "https://api.anthropic.com");
            break;
        default:
            snprintf(url, WLT_AI_ENDPOINT_LEN + WLT_AI_API_KEY_LEN + 64,
                "%s/v1/chat/completions",
                cfg->cloud_endpoint);
            break;
        }
    } else {
        snprintf(url, WLT_AI_ENDPOINT_LEN + WLT_AI_API_KEY_LEN + 64,
            "%s/v1/chat/completions",
            cfg->local_vlm_endpoint[0] ? cfg->local_vlm_endpoint
                                       : "http://localhost:8081");
    }

    /* Perform the HTTP request using wlt_http_post (no libcurl dependency).
     * Same pattern as ai.c's local LLM path. */
    http_code = wlt_http_post(url, json_body, "application/json",
                             resp_buf, sizeof(resp_buf), 30);

    if (http_code < 0) {
        fprintf(stderr, "wlt-ai: VLM connection error\n");
        ret = WLT_AI_ERR_NETWORK;
        goto cleanup;
    }

    if (http_code != 200 || resp_buf[0] == '\0') {
        fprintf(stderr, "wlt-ai: VLM HTTP %d\n", http_code);
        ret = WLT_AI_ERR_MODEL;
        goto cleanup;
    }

    /* Extract the response text from JSON */
    if (wlt_extract_json_text(resp_buf, result, result_len) == NULL) {
        strncpy(result, resp_buf, result_len - 1);
        result[result_len - 1] = '\0';
    }

    ret = WLT_AI_OK;

cleanup:
    if (img_file)
        fclose(img_file);
    if (img_data)
        free(img_data);
    if (b64_data)
        free(b64_data);
    if (json_body)
        free(json_body);
    if (url)
        free(url);
    return ret;
}

wlt_ai_err_t
wlt_vision_describe(const wlt_ai_config_t *cfg,
                    char *result, size_t result_len)
{
    wlt_ai_err_t err;

    err = wlt_vision_capture(WLT_AI_SCREENSHOT_PATH);
    if (err != WLT_AI_OK) {
        strncpy(result, "[Screen capture failed]", result_len - 1);
        result[result_len - 1] = '\0';
        return err;
    }

    err = wlt_vision_analyze(WLT_AI_SCREENSHOT_PATH,
                             "Describe what is currently on the screen. "
                             "Include window titles, active applications, "
                             "and any notable UI elements.",
                             cfg, result, result_len);

    /* Clean up the temporary screenshot */
    unlink(WLT_AI_SCREENSHOT_PATH);

    return err;
}
