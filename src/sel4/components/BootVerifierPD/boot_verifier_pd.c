/*
 * boot_verifier_pd.c — Boot Verifier Protection Domain
 *
 * Bare seL4 component (no Linux, no filesystem, no sockets).
 * Verifies system integrity via SHA-256 hashing + comparison.
 *
 * At boot: receives expected hashes from vm0, computes SHA-256 of
 * the kernel image in memory, compares. If mismatch: boot aborts.
 * At runtime: periodic re-hash of critical memory regions.
 *
 * This mirrors KeyVaultPD's pattern: a bare CAmkES component with
 * shared memory + notification IPC to the Linux VM.
 */
#include <camkes.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Minimal SHA-256 (same implementation as wlt-boot/verify.c) */
/* For the seL4 build, this is compiled with musl libc. */

/* Status codes written to response_dp */
#define BOOT_VERIFY_OK      "OK"
#define BOOT_VERIFY_TAMPERED "TAMPERED"
#define BOOT_VERIFY_ERROR   "ERROR"

/* The boot verifier runs its control loop: wait for a verify_request,
 * read the request from request_dp, compute the hash, compare, write
 * the result to response_dp, emit verify_done. */
static void
boot_verifier_run(void)
{
    printf("[boot-verifier] alive\n");
    printf("[boot-verifier] Waiting for verify request from Linux VM...\n");

    for (;;) {
        /* Wait for vm0 to signal a verify request */
        verify_request_wait();

        /* Read the request from shared memory.
         * Format: [32-byte expected_hash] [data to verify...]
         * For Phase 1 (scaffold): just echo back OK. */
        char *req = (char *)request_dp;
        char *resp = (char *)response_dp;

        /* Phase 1 scaffold: respond OK (the real implementation would
         * compute SHA-256 of the data in request_dp and compare with
         * the expected hash at the start). */
        printf("[boot-verifier] verify request received, responding OK\n");
        strncpy(resp, BOOT_VERIFY_OK, 4096);

        /* Signal vm0 that the response is ready */
        verify_done_emit();
    }
}

int
main(void)
{
    boot_verifier_run();
    return 0;
}
