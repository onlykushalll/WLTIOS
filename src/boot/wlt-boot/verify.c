/**
 * @file verify.c
 * @brief Signature verification implementation
 * @module wlt-boot/verify
 * @purpose SHA-256 + Ed25519 verification for seL4 kernel
 * @spec: docs/specs/wlt-boot.md
 */

#include "verify.h"
#include "console.h"

wlt_boot_error_t
wlt_verify_kernel(const uint8_t *kernel_data,
                   size_t kernel_size,
                   const uint8_t *sig_data,
                   size_t sig_size)
{
    /* TODO(wlt-boot): Implement kernel signature verification */
    /* Steps: */
    /*   1. Compute SHA-256 of kernel_data */
    /*   2. Verify Ed25519 signature of the hash */
    /*   3. Return WLT_BOOT_OK or WLT_BOOT_ERR_SIGNATURE */
    (void)kernel_data;
    (void)kernel_size;
    (void)sig_data;
    (void)sig_size;
    return WLT_BOOT_OK;
}

void
wlt_verify_sha256(const uint8_t *data, size_t size, uint8_t hash[32])
{
    /* TODO(wlt-boot): Implement SHA-256 */
    /* Either: */
    /*   - Inline implementation (public domain SHA-256) */
    /*   - Or UEFI HashProtocol if available */
    (void)data;
    (void)size;
    (void)hash;
}

wlt_boot_error_t
wlt_verify_ed25519(const uint8_t *data, size_t data_size,
                    const uint8_t sig[64],
                    const uint8_t pubkey[32])
{
    /* TODO(wlt-boot): Implement Ed25519 verification */
    /* Use a minimal Ed25519 implementation (e.g., from monocypher) */
    (void)data;
    (void)data_size;
    (void)sig;
    (void)pubkey;
    return WLT_BOOT_OK;
}
