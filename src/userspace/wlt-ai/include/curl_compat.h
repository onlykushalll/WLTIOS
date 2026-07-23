/**
 * @file curl_compat.h
 * @brief Curl compatibility shim — allows ai.c to compile without libcurl
 *
 * Defines curl types and functions as no-ops/stubs so ai.c's existing
 * cloud/STT/TTS code compiles. The local LLM path uses wlt_http_post
 * instead (added via #define replacement).
 *
 * This is the pragmatic approach: don't rewrite 900 lines of working
 * code, just make the curl calls compile as no-ops.
 */
#ifndef CURL_COMPAT_H
#define CURL_COMPAT_H

/* Type definitions */
typedef void CURL;
typedef int CURLcode;
typedef int CURLoption;
typedef int CURLINFO;
struct curl_slist { char *data; struct curl_slist *next; };

/* Constants */
#define CURLE_OK 0
#define CURL_GLOBAL_DEFAULT 0
#define CURLOPT_URL 10000
#define CURLOPT_POSTFIELDS 10015
#define CURLOPT_POSTFIELDSIZE 10016
#define CURLOPT_WRITEFUNCTION 20011
#define CURLOPT_WRITEDATA 10001
#define CURLOPT_HTTPHEADER 10023
#define CURLOPT_TIMEOUT 13
#define CURLOPT_CONNECTTIMEOUT 67
#define CURLOPT_CUSTOMREQUEST 10036
#define CURLINFO_RESPONSE_CODE 0x200002

/* Function stubs — all return success/empty */
/* Returns non-NULL dummy so wlt_ai_init() doesn't abort.
 * The local LLM path uses wlt_http_post (raw socket), not curl.
 * Cloud/STT/TTS paths are stubs that compile but don't execute. */
static inline CURL *curl_easy_init(void) { return (CURL *)1; }
static inline void curl_easy_cleanup(CURL *c) { (void)c; }
static inline void curl_easy_reset(CURL *c) { (void)c; }
static inline CURLcode curl_easy_setopt(CURL *c, CURLoption o, ...) { (void)c; (void)o; return CURLE_OK; }
static inline CURLcode curl_easy_perform(CURL *c) {
    (void)c;
    /* FAIL-CLOSED: the cloud/STT/TTS paths are stubbed out. Return a
     * non-OK code so ai.c's error handling fires honestly with
     * 'curl error: stub' rather than silently pretending to succeed
     * and then reading an uninitialized HTTP code. When real libcurl
     is linked (-lcurl), these stubs are not used and real calls run. */
    return 1; /* non-CURLE_OK */
}
static inline void curl_global_init(long f) { (void)f; }
static inline void curl_global_cleanup(void) {}
static inline struct curl_slist *curl_slist_append(struct curl_slist *l, const char *s) { (void)s; return l; }
static inline void curl_slist_free_all(struct curl_slist *l) { (void)l; }
static inline const char *curl_easy_strerror(CURLcode c) { (void)c; return "stub"; }
static inline CURLcode curl_easy_getinfo(CURL *c, CURLINFO o, ...) {
    (void)c; (void)o;
    /* Set the response code to 0 (will cause "HTTP 0" error, which is correct — stub) */
    return CURLE_OK;
}

#endif /* CURL_COMPAT_H */
