/**
 * @file verify.h
 * @brief Signature verification for WLT-Boot
 * @module wlt-boot/verify
 * @purpose Verifies seL4 kernel signature (SHA-256 + Ed25519)
 * @dependencies: boot.h
 * @spec: docs/specs/wlt-boot.md
 * @adr: docs/adr/01-uefi-vs-bios.md (verified boot)
 */

#ifndef WLT_VERIFY_H
#define WLT_VERIFY_H

#include "boot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Verify kernel signature
 * @param kernel_data    Kernel binary data
 * @param kernel_size    Kernel size
 * @param sig_data       Signature data
 * @param sig_size       Signature size
 * @return WLT_BOOT_OK if signature valid
 */
wlt_boot_error_t wlt_verify_kernel(const uint8_t *kernel_data,
                                    size_t kernel_size,
                                    const uint8_t *sig_data,
                                    size_t sig_size);

/**
 * Compute SHA-256 hash
 * @param data   Input data
 * @param size   Data size
 * @param hash   Output 32-byte hash
 */
void wlt_verify_sha256(const uint8_t *data, size_t size, uint8_t hash[32]);

/**
 * Verify Ed25519 signature
 * @param data       Signed data
 * @param data_size  Data size
 * @param sig        64-byte signature
 * @param pubkey     32-byte public key
 * @return WLT_BOOT_OK if valid
 */
wlt_boot_error_t wlt_verify_ed25519(const uint8_t *data, size_t data_size,
                                     const uint8_t sig[64],
                                     const uint8_t pubkey[32]);

#ifdef __cplusplus
}
#endif

#endif /* WLT_VERIFY_H */
