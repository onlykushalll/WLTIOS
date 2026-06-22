/**
 * @file transaction.c
 * @brief Atomic transaction — all-or-nothing package operations
 * @module wlt-pkg/transaction
 * @purpose Stage → verify → install → commit or rollback
 * @dependencies: pkg.h
 * @spec: docs/specs/wlt-pkg.md
 */

#include "pkg.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

typedef enum {
    WLT_PKG_TXN_IDLE = 0,
    WLT_PKG_TXN_STAGING,
    WLT_PKG_TXN_VERIFYING,
    WLT_PKG_TXN_INSTALLING,
    WLT_PKG_TXN_COMMITTED,
    WLT_PKG_TXN_FAILED,
    WLT_PKG_TXN_ROLLED_BACK,
} wlt_pkg_txn_state_t;

typedef struct {
    wlt_pkg_txn_state_t state;
    char staging_dir[WLT_PKG_MAX_PATH];
    char backup_dir[WLT_PKG_MAX_PATH];
    int operations_count;
} wlt_pkg_transaction_t;

static wlt_pkg_transaction_t g_txn;

wlt_pkg_error_t
wlt_pkg_transaction_begin(void)
{
    if (g_txn.state != WLT_PKG_TXN_IDLE)
        return WLT_PKG_ERR_CONFLICT;

    memset(&g_txn, 0, sizeof(g_txn));
    g_txn.state = WLT_PKG_TXN_STAGING;

    /* Create staging directory */
    snprintf(g_txn.staging_dir, sizeof(g_txn.staging_dir),
             "/var/wlt/store/.staging-%d", getpid());
    mkdir(g_txn.staging_dir, 0755);

    /* Create backup directory (for rollback) */
    snprintf(g_txn.backup_dir, sizeof(g_txn.backup_dir),
             "/var/wlt/store/.backup-%d", getpid());
    mkdir(g_txn.backup_dir, 0755);

    g_txn.operations_count = 0;
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_transaction_stage_file(const char *src, const char *dest_name)
{
    if (g_txn.state != WLT_PKG_TXN_STAGING)
        return WLT_PKG_ERR_INVALID_ARG;

    /* TODO(wlt-pkg): Copy file to staging directory */
    (void)src;
    (void)dest_name;
    g_txn.operations_count++;

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_transaction_verify(void)
{
    if (g_txn.state != WLT_PKG_TXN_STAGING)
        return WLT_PKG_ERR_INVALID_ARG;

    g_txn.state = WLT_PKG_TXN_VERIFYING;

    /* TODO(wlt-pkg): Verify all staged files (hashes, signatures) */
    /* If verification fails: */
    /*   g_txn.state = WLT_PKG_TXN_FAILED; */
    /*   return WLT_PKG_ERR_SIGNATURE; */

    g_txn.state = WLT_PKG_TXN_INSTALLING;
    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_transaction_commit(void)
{
    if (g_txn.state != WLT_PKG_TXN_INSTALLING)
        return WLT_PKG_ERR_INVALID_ARG;

    /* TODO(wlt-pkg): Atomically move staging → store, create symlinks */
    /* This is the point of no return */

    /* Clean up staging + backup */
    rmdir(g_txn.staging_dir);
    rmdir(g_txn.backup_dir);

    g_txn.state = WLT_PKG_TXN_COMMITTED;
    memset(&g_txn, 0, sizeof(g_txn));
    g_txn.state = WLT_PKG_TXN_IDLE;

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_transaction_rollback(void)
{
    if (g_txn.state == WLT_PKG_TXN_IDLE ||
        g_txn.state == WLT_PKG_TXN_COMMITTED)
        return WLT_PKG_ERR_INVALID_ARG;

    /* TODO(wlt-pkg): Restore from backup, remove staging */
    /* Restore all files from backup_dir */
    /* Remove staging_dir */

    rmdir(g_txn.staging_dir);
    rmdir(g_txn.backup_dir);

    g_txn.state = WLT_PKG_TXN_ROLLED_BACK;
    memset(&g_txn, 0, sizeof(g_txn));
    g_txn.state = WLT_PKG_TXN_IDLE;

    return WLT_PKG_OK;
}

wlt_pkg_error_t
wlt_pkg_transaction_is_active(bool *active)
{
    if (active == NULL)
        return WLT_PKG_ERR_INVALID_ARG;

    *active = (g_txn.state != WLT_PKG_TXN_IDLE);
    return WLT_PKG_OK;
}
