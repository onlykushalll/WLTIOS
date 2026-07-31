#ifndef WLT_HTTP_TOR_H
#define WLT_HTTP_TOR_H

/* HTTP GET/POST through Tor SOCKS5 proxy (127.0.0.1:9050).
 * No direct connection, no DNS leak — hostname is resolved by Tor.
 * Returns HTTP status code or -1 on error. */
int wlt_http_get_tor(const char *url, char *response, int response_size, int timeout_sec);
int wlt_http_post_tor(const char *url, const char *body, const char *content_type,
                      char *response, int response_size, int timeout_sec);

#endif
