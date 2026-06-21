/**
 * @file pty.h
 * @brief PTY (pseudo-terminal) management for WLT-Terminal
 * @module wlt-terminal/pty
 * @purpose Allocates a master/slave PTY pair and runs the shell process
 * @dependencies: terminal.h
 * @tested_by: tests/userspace/pty_test.c
 * @spec: docs/specs/wlt-terminal.md
 */

#ifndef WLT_PTY_H
#define WLT_PTY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ──────────────────────────────────────── */

#define WLT_PTY_MAX_ARGS 32
#define WLT_PTY_READ_BUF 8192
#define WLT_PTY_DEFAULT_SHELL "/bin/sh"
#define WLT_PTY_MAX_SHELL_PATH 256

/* ─── Error Codes ────────────────────────────────────── */

typedef enum {
    WLT_PTY_OK = 0,
    WLT_PTY_ERR_INVALID_ARG = -1,
    WLT_PTY_ERR_OPENPTY = -2,
    WLT_PTY_ERR_FORK = -3,
    WLT_PTY_ERR_EXEC = -4,
    WLT_PTY_ERR_IO = -5,
    WLT_PTY_ERR_CLOSED = -6,
    WLT_PTY_ERR_NOMEM = -7,
} wlt_pty_error_t;

/* ─── PTY State ──────────────────────────────────────── */

/**
 * PTY state. The master fd is held by the terminal; the child
 * process runs the shell with the slave end as its controlling tty.
 *
 * The typedef is repeated from terminal.h so pty.h is usable alone;
 * C11 permits duplicate typedefs that name the same type.
 */
typedef struct wlt_Pty wlt_Pty;
struct wlt_Pty {
    int master_fd;          /* Read/write to shell */
    pid_t child_pid;        /* Shell process, -1 if not running */
    bool closed;
    uint32_t cols;
    uint32_t rows;
    char shell[WLT_PTY_MAX_SHELL_PATH];
    int child_status;       /* Exit status if exited */
};

/* ─── Public API ─────────────────────────────────────── */

/**
 * Spawn a shell on a new PTY with the given window size.
 * @param shell   Path to shell binary (NULL -> /bin/sh)
 * @param cols    Initial column count
 * @param rows    Initial row count
 * @param out     Receives new PTY handle
 */
wlt_pty_error_t
wlt_pty_create(wlt_Pty **out,
               const char *shell,
               uint32_t cols,
               uint32_t rows);

/**
 * Close the PTY master fd and reap the shell process.
 */
void
wlt_pty_destroy(wlt_Pty *pty);

/**
 * Read shell output. Non-blocking.
 * Returns WLT_PTY_ERR_CLOSED when the shell has exited.
 * @param out_len  Number of bytes actually read (may be 0)
 */
wlt_pty_error_t
wlt_pty_read(wlt_Pty *pty,
             uint8_t *buf,
             size_t buf_len,
             size_t *out_len);

/**
 * Write user input to the shell.
 */
wlt_pty_error_t
wlt_pty_write(wlt_Pty *pty,
              const uint8_t *buf,
              size_t len);

/**
 * Notify the shell of a window size change (sends SIGWINCH).
 */
wlt_pty_error_t
wlt_pty_resize(wlt_Pty *pty,
               uint32_t cols,
               uint32_t rows);

/**
 * Check if the child process has exited.
 * Reaps the child if it has, populating child_status.
 */
bool
wlt_pty_child_exited(wlt_Pty *pty);

/**
 * Get the master fd for poll/select integration.
 * @return -1 if closed
 */
int
wlt_pty_fd(const wlt_Pty *pty);

/**
 * Get the child PID (or -1 if not running).
 */
pid_t
wlt_pty_pid(const wlt_Pty *pty);

const char *
wlt_pty_error_str(wlt_pty_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_PTY_H */
