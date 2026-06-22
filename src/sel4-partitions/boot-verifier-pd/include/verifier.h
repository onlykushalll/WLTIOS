/**
 * @file verifier.h
 * @brief Public interface for Boot Verifier seL4 Protection Domain
 * @module boot-verifier-pd
 * @purpose Verify system integrity at boot + runtime via memory region hashing
 * @dependencies: seL4 Microkit
 * @spec: docs/specs/boot-verifier-pd.md
 * @adr: docs/adr/02-sel4-as-base.md
 *
 * ARCHITECTURE: This is a bare-metal seL4 PD.
 * NO libc, NO filesystem, NO sockets.
 * Can only hash MEMORY REGIONS mapped into it (not files on disk).
 * Runtime detection is limited to memory contents.
 */

#ifndef WLT_VERIFIER_PD_H
#define WLT_VERIFIER_PD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BV_MAX_REGIONS 16
#define BV_HASH_SIZE 32
#define BV_MAX_NAME 64

typedef enum {
    BV_OK = 0,
    BV_ERR_INVALID = -1,
    BV_ERR_NOT_FOUND = -2,
    BV_ERR_TAMPERED = -3,
    BV_ERR_FULL = -4,
} bv_error_t;

typedef enum {
    BV_OP_VERIFY_BOOT = 0,
    BV_OP_CHECK_RUNTIME = 1,
    BV_OP_GET_STATUS = 2,
    BV_OP_ALERT = 3,
} bv_op_t;

typedef struct {
    char name[BV_MAX_NAME];
    uint8_t *addr;       /* Memory region address (mapped by Microkit) */
    size_t size;          /* Region size in bytes */
    uint8_t boot_hash[BV_HASH_SIZE];  /* SHA-256 taken at boot */
    bool verified;
} bv_region_t;

typedef struct {
    bv_op_t op;
    int32_t status;
    size_t region_count;
    bool all_verified;
    bool tamper_detected;
} bv_ipc_header_t;

/* PD entry points */
void init(void);
void notified(microkit_channel ch);
void protected(microkit_channel ch, microkit_msginfo msg);

/* Internal API */
bv_error_t bv_verify_boot(void);
bv_error_t bv_check_runtime(void);
bv_error_t bv_register_region(const char *name, uint8_t *addr, size_t size);
bv_error_t bv_get_status(bool *all_verified, bool *tamper_detected);

#ifdef __cplusplus
}
#endif

#endif /* WLT_VERIFIER_PD_H */
