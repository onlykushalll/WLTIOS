/**
 * @file verifier.c
 * @brief Main Boot Verifier PD — Microkit init/notified, hash memory, alert
 * @module boot-verifier-pd
 * @purpose Verify system integrity by hashing memory regions
 * @dependencies: verifier.h, hashes.c
 * @spec: docs/specs/boot-verifier-pd.md
 * @adr: docs/adr/02-sel4-as-base.md
 *
 * BARE METAL: No libc, no malloc, no filesystem.
 * Can only hash memory regions mapped into this PD at boot.
 * Cannot access files on disk (no filesystem).
 * Runtime detection limited to memory contents.
 */

#include "verifier.h"
#include <string.h>

/* Microkit stubs */
#define microkit_channel int
#define microkit_msginfo uint64_t
#define MICROKIT_NOTIFY(ch) ((void)0)

static bv_region_t g_regions[BV_MAX_REGIONS];
static size_t g_region_count = 0;
static bool g_tamper_detected = false;
static bool g_initialized = false;

void
init(void)
{
    memset(g_regions, 0, sizeof(g_regions));
    g_region_count = 0;
    g_tamper_detected = false;
    g_initialized = true;

    /* TODO(bv-pd): Register memory regions that seL4 maps into this PD:
     *   - seL4 kernel image
     *   - capdl-loader image
     *   - Linux kernel image (if accessible)
     *   - Other PD binaries
     */

    /* Take boot-time hashes */
    bv_verify_boot();
}

void
notified(microkit_channel ch)
{
    /* Handle IPC request */
    bv_ipc_header_t *hdr = (bv_ipc_header_t *)0;  /* Would be in shared memory */

    switch (hdr->op)
    {
        case BV_OP_VERIFY_BOOT:
            hdr->status = bv_verify_boot();
            break;
        case BV_OP_CHECK_RUNTIME:
            hdr->status = bv_check_runtime();
            break;
        case BV_OP_GET_STATUS:
            bv_get_status(&hdr->all_verified, &hdr->tamper_detected);
            hdr->status = BV_OK;
            break;
        case BV_OP_ALERT:
            /* Acknowledge alert */
            hdr->status = BV_OK;
            break;
        default:
            hdr->status = BV_ERR_INVALID;
    }

    hdr->region_count = g_region_count;
    MICROKIT_NOTIFY(ch);
}

void
protected(microkit_channel ch, microkit_msginfo msg)
{
    notified(ch);
}

bv_error_t
bv_verify_boot(void)
{
    if (!g_initialized)
        return BV_ERR_INVALID;

    /* Hash all registered regions and store as boot_hash */
    for (size_t i = 0; i < g_region_count; i++)
    {
        bv_region_t *r = &g_regions[i];
        bv_sha256(r->addr, r->size, r->boot_hash);
        r->verified = true;
    }

    return BV_OK;
}

bv_error_t
bv_check_runtime(void)
{
    if (!g_initialized)
        return BV_ERR_INVALID;

    /* Re-hash all regions and compare to boot_hash */
    for (size_t i = 0; i < g_region_count; i++)
    {
        bv_region_t *r = &g_regions[i];
        uint8_t current_hash[BV_HASH_SIZE];
        bv_sha256(r->addr, r->size, current_hash);

        if (memcmp(current_hash, r->boot_hash, BV_HASH_SIZE) != 0)
        {
            /* TAMPER DETECTED! */
            g_tamper_detected = true;
            r->verified = false;

            /* Zero the current hash (security) */
            memset(current_hash, 0, BV_HASH_SIZE);

            /* TODO(bv-pd): Alert WLT-Init via IPC */
            /* MICROKIT_NOTIFY(ALERT_CHANNEL); */

            return BV_ERR_TAMPERED;
        }

        memset(current_hash, 0, BV_HASH_SIZE);
    }

    return BV_OK;
}

bv_error_t
bv_register_region(const char *name, uint8_t *addr, size_t size)
{
    if (name == NULL || addr == NULL || size == 0)
        return BV_ERR_INVALID;

    if (g_region_count >= BV_MAX_REGIONS)
        return BV_ERR_FULL;

    bv_region_t *r = &g_regions[g_region_count];
    memset(r, 0, sizeof(*r));
    strncpy(r->name, name, BV_MAX_NAME - 1);
    r->addr = addr;
    r->size = size;
    r->verified = false;

    g_region_count++;
    return BV_OK;
}

bv_error_t
bv_get_status(bool *all_verified, bool *tamper_detected)
{
    if (all_verified == NULL || tamper_detected == NULL)
        return BV_ERR_INVALID;

    *all_verified = true;
    *tamper_detected = g_tamper_detected;

    for (size_t i = 0; i < g_region_count; i++)
    {
        if (!g_regions[i].verified)
            *all_verified = false;
    }

    return BV_OK;
}
