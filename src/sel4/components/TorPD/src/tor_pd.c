/*
 * tor_pd.c — Minimal Tor protection domain (Phase 1: IPC proof)
 *
 * This is a seL4 component (NOT a Linux process) that runs in its own
 * protection domain. It communicates with the Linux VM via shared memory
 * and notifications, proving seL4 partition isolation.
 *
 * Phase 1: ping/pong proof (mirrors KeyVaultPD's first milestone)
 * Phase 2 (future): Tor daemon runs in a second Linux guest VM
 *
 * Spec: docs/specs/wlt-tor-pd.md
 * ADR: docs/adr/09-tor-pd-scaffold.md
 */

#include <stdio.h>
#include <string.h>
#include <camkes.h>

/* Shared memory buffer (defined by CAmkES dataport) */
#define BUF_SIZE 4096

/* Called when the VM signals us (ready_wait blocks until notification) */
int run(void)
{
    /* Print to serial — this goes to the seL4 serial output, NOT Linux's */
    printf("[tor-pd] alive\n");
    printf("[tor-pd] Waiting for ping from Linux VM...\n");

    while (1) {
        /* Block until the Linux VM signals us */
        ready_wait();

        /* Read the ping from shared memory */
        volatile char *src = (volatile char *)src_dp;
        volatile char *dst = (volatile char *)dest_dp;

        /* Check if it's a ping */
        if (strncmp((char *)src, "ping", 4) == 0) {
            /* Write pong to the destination shared memory */
            strncpy((char *)dst, "pong", 5);
            printf("[tor-pd] ping received, pong sent\n");
        } else {
            /* Echo back for testing */
            int len = strnlen((char *)src, BUF_SIZE - 1);
            memcpy((char *)dst, (char *)src, len);
            dst[len] = 0;
            printf("[tor-pd] echo: %.*s\n", len, (char *)src);
        }

        /* Signal the VM that we're done */
        done_emit();
    }

    return 0;
}
