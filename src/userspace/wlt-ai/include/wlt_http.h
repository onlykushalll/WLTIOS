/**
 * @file wlt_http.h
 * @brief Minimal HTTP client (replaces libcurl for loopback-only HTTP)
 */
#ifndef WLT_HTTP_H
#define WLT_HTTP_H

/* HTTP POST: returns HTTP status code (200, 404, etc.) or -1 on error.
 * Response body (without headers) is written to response_buf. */
int wlt_http_post(const char *url, const char *body, const char *content_type,
                  char *response_buf, int response_size, int timeout_sec);

/* HTTP GET: returns HTTP status code or -1 on error. */
int wlt_http_get(const char *url, char *response_buf, int response_size, int timeout_sec);

#endif
