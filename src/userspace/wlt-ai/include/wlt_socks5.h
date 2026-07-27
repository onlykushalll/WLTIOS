/**
 * @file wlt_socks5.h — SOCKS5 proxy support for Tor routing
 * @purpose Lets wlt-http connect through Tor's SOCKS5 proxy (127.0.0.1:9050)
 *          instead of directly. This is what makes the Tor Browser actually
 *          route through Tor — without it, wlt_http.c connects directly,
 *          bypassing Tor entirely (the exact bug the audit found).
 *
 * SOCKS5 protocol (RFC 1928):
 *   1. Client → proxy: greeting (version=5, nmethods=1, method=0=no-auth)
 *   2. Proxy → client: select (version=5, method=0)
 *   3. Client → proxy: connect request (version=5, cmd=1, atyp=3=domain,
 *      domain_len, domain, port_hi, port_lo)
 *   4. Proxy → client: reply (version=5, rep=0=success, atyp, addr, port)
 *   5. Tunnel established — send HTTP over the same socket.
 */
#ifndef WLT_SOCKS5_H
#define WLT_SOCKS5_H

#include <stdint.h>
#include <stddef.h>

/* Connect to a host:port through Tor's SOCKS5 proxy at 127.0.0.1:9050.
 * Returns a connected socket fd on success, -1 on failure.
 * The caller then does HTTP over this socket (send/recv).
 * hostname is resolved BY TOR (not locally) — no DNS leak. */
int wlt_socks5_connect(const char *hostname, int port);

/* Check if Tor's SOCKS5 proxy is available (127.0.0.1:9050).
 * Returns 1 if available, 0 if not. */
int wlt_socks5_available(void);

#endif /* WLT_SOCKS5_H */
