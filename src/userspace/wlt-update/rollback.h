/**
 * @file rollback.h
 * @brief Anti-rollback counter for the A/B update system
 * @purpose Closes research-I gap G2/R2: without a tamper-evident
 *          monotonic counter, an attacker can re-serve an old (validly
 *          signed) package to downgrade a device to a vulnerable version.
 *
 * Design (Android-AVB model, MVP):
 *   - The counter is a uint64 stored at WLT_UPDATE_ROLLBACK_PATH.
 *   - The file is <decimal-counter>\n<base64-HMAC-SHA256(counter, key)>.
 *   - The HMAC key is a build-time constant for now (clearly marked as
 *     needing KeyVaultPD delivery — see research-H). This prevents trivial
 *     counter-file tampering by an attacker who can write the file but
 *     doesn't have the source/binary. A determined attacker with root +
 *     the binary can extract the key; the full fix is TPM2 NV or
 *     seL4-owned reserved flash (documented in research-I R2).
 *   - On install: if the package's rollback_index < stored counter,
 *     refuse with WLT_UPDATE_ERR_DOWNGRADE. On success, bump the counter.
 *
 * @dependencies: libsodium (HMAC-SHA256 via crypto_auth)
 * @spec: docs/specs/wlt-update.md, research-I (worklog)
 */
#ifndef WLT_ROLLBACK_H
#define WLT_ROLLBACK_H

#include <stdint.h>
#include "update.h"

/* Load the stored rollback counter. If the file doesn't exist, returns 0
 * (no updates have ever been installed — first install is allowed). If the
 * file exists but the HMAC doesn't verify, returns WLT_UPDATE_ERR_IO
 * (tamper-evident: refuse rather than silently resetting). */
wlt_update_error_t wlt_rollback_load(uint64_t *out_counter);

/* Store a new counter value atomically (O_EXCL|O_NOFOLLOW|fsync).
 * Called after a successful install to bump the counter. */
wlt_update_error_t wlt_rollback_store(uint64_t counter);

/* Check whether a package's rollback_index is allowed (>= stored).
 * Returns WLT_UPDATE_OK if allowed, WLT_UPDATE_ERR_DOWNGRADE if refused. */
wlt_update_error_t wlt_rollback_check(uint64_t package_index);

/* Convenience: bump the counter to package_index if it's higher.
 * Called after a successful install. No-op if package_index <= stored. */
wlt_update_error_t wlt_rollback_bump(uint64_t package_index);

#endif /* WLT_ROLLBACK_H */
