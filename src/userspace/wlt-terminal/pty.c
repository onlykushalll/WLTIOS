/**
 * @file pty.c
 * @brief PTY management — openpty, fork, execvp shell, read/write
 * @module wlt-terminal/pty
 * @purpose Spawns the user's shell on a pseudo-terminal and proxies I/O
 * @dependencies: pty.h, terminal.h
 * @tested_by: tests/userspace/pty_test.c
 * @spec: docs/specs/wlt-terminal.md
 */

#include "pty.h"
#include "terminal.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>

/* openpty() is provided by libutil (-lutil). glibc/musl declare it in
 * <pty.h> / <util.h>, but this module's own include/pty.h shadows the
 * system header of the same name once -Iinclude is in effect. Declare the
 * prototype directly instead (stable libutil ABI, both libcs). */
int openpty(int *amaster, int *aslave, char *name,
            const struct termios *termp, const struct winsize *winp);

/* ─── Helpers ────────────────────────────────────────── */

static void
set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void
apply_window_size(int fd, uint32_t cols, uint32_t rows)
{
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)cols;
    ws.ws_row = (unsigned short)rows;
    ws.ws_xpixel = (unsigned short)(cols * 8);
    ws.ws_ypixel = (unsigned short)(rows * 16);
    (void)ioctl(fd, TIOCSWINSZ, &ws);
}

/* ─── Child Process ──────────────────────────────────── */

/**
 * In the child: set up controlling tty, environment, then execvp.
 * Never returns on success; calls _exit on failure.
 */
static void
child_exec(int slave_fd, const char *shell, uint32_t cols, uint32_t rows)
{
    /* New session so we get a controlling tty */
    if (setsid() < 0)
    {
        _exit(127);
    }

    /* Slave becomes stdin/stdout/stderr */
    if (dup2(slave_fd, STDIN_FILENO) < 0 ||
        dup2(slave_fd, STDOUT_FILENO) < 0 ||
        dup2(slave_fd, STDERR_FILENO) < 0)
    {
        _exit(127);
    }
    if (slave_fd > STDERR_FILENO)
        close(slave_fd);

    /* Set window size so the shell knows our dimensions */
    apply_window_size(STDIN_FILENO, cols, rows);

    /* Basic terminal environment */
    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);
    setenv("LINES", "", 0);  /* Let the shell query the tty */
    setenv("COLUMNS", "", 0);

    /* Build argv: shell -l (login shell) */
    char *argv[4];
    argv[0] = (char *)shell;
    argv[1] = "-l";
    argv[2] = NULL;

    execvp(shell, argv);

    /* If we get here, exec failed */
    _exit(127);
}

/* ─── Public API ─────────────────────────────────────── */

wlt_pty_error_t
wlt_pty_create(wlt_Pty **out,
               const char *shell,
               uint32_t cols,
               uint32_t rows)
{
    if (out == NULL)
        return WLT_PTY_ERR_INVALID_ARG;
    if (cols == 0 || rows == 0)
        return WLT_PTY_ERR_INVALID_ARG;

    wlt_Pty *pty = calloc(1, sizeof(*pty));
    if (pty == NULL)
        return WLT_PTY_ERR_NOMEM;

    pty->master_fd = -1;
    pty->child_pid = -1;
    pty->closed = false;
    pty->cols = cols;
    pty->rows = rows;
    pty->child_status = 0;

    const char *sh = (shell != NULL && shell[0] != '\0')
        ? shell : WLT_PTY_DEFAULT_SHELL;
    strncpy(pty->shell, sh, sizeof(pty->shell) - 1);
    pty->shell[sizeof(pty->shell) - 1] = '\0';

    int master_fd = -1;
    int slave_fd = -1;
    if (openpty(&master_fd, &slave_fd, NULL, NULL, NULL) < 0)
    {
        free(pty);
        return WLT_PTY_ERR_OPENPTY;
    }

    /* Apply window size before fork so the shell sees it on start */
    apply_window_size(slave_fd, cols, rows);

    pid_t pid = fork();
    if (pid < 0)
    {
        close(master_fd);
        close(slave_fd);
        free(pty);
        return WLT_PTY_ERR_FORK;
    }

    if (pid == 0)
    {
        /* Child — close master, exec shell on slave */
        close(master_fd);
        child_exec(slave_fd, pty->shell, cols, rows);
        /* never returns */
    }

    /* Parent */
    close(slave_fd);
    set_nonblock(master_fd);

    pty->master_fd = master_fd;
    pty->child_pid = pid;
    *out = pty;
    return WLT_PTY_OK;
}

void
wlt_pty_destroy(wlt_Pty *pty)
{
    if (pty == NULL) return;

    if (pty->child_pid > 0 && !pty->closed)
    {
        /* Politely ask the shell to exit */
        (void)kill(pty->child_pid, SIGHUP);
        /* Wait briefly, then SIGKILL */
        for (int i = 0; i < 20; i++)
        {
            pid_t r = waitpid(pty->child_pid, &pty->child_status, WNOHANG);
            if (r == pty->child_pid) break;
            if (r < 0) break;
            usleep(50000);  /* 50ms */
        }
        if (waitpid(pty->child_pid, &pty->child_status, WNOHANG) == 0)
        {
            (void)kill(pty->child_pid, SIGKILL);
            (void)waitpid(pty->child_pid, &pty->child_status, 0);
        }
    }

    if (pty->master_fd >= 0)
        close(pty->master_fd);

    pty->master_fd = -1;
    pty->child_pid = -1;
    pty->closed = true;
    free(pty);
}

wlt_pty_error_t
wlt_pty_read(wlt_Pty *pty,
             uint8_t *buf,
             size_t buf_len,
             size_t *out_len)
{
    if (pty == NULL || buf == NULL)
        return WLT_PTY_ERR_INVALID_ARG;
    if (out_len != NULL) *out_len = 0;
    if (pty->closed || pty->master_fd < 0)
        return WLT_PTY_ERR_CLOSED;

    ssize_t n = read(pty->master_fd, buf, buf_len);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return WLT_PTY_OK;
        if (errno == EIO)
        {
            /* Shell exited */
            pty->closed = true;
            return WLT_PTY_ERR_CLOSED;
        }
        return WLT_PTY_ERR_IO;
    }
    if (n == 0)
    {
        pty->closed = true;
        return WLT_PTY_ERR_CLOSED;
    }

    if (out_len != NULL) *out_len = (size_t)n;
    return WLT_PTY_OK;
}

wlt_pty_error_t
wlt_pty_write(wlt_Pty *pty,
              const uint8_t *buf,
              size_t len)
{
    if (pty == NULL || (buf == NULL && len > 0))
        return WLT_PTY_ERR_INVALID_ARG;
    if (pty->closed || pty->master_fd < 0)
        return WLT_PTY_ERR_CLOSED;
    if (len == 0) return WLT_PTY_OK;

    size_t off = 0;
    while (off < len)
    {
        ssize_t n = write(pty->master_fd, buf + off, len - off);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* Master buffer is full — yield. In a real compositor
                 * we would integrate with the event loop. For now,
                 * retry once after a brief sleep. */
                usleep(1000);
                continue;
            }
            if (errno == EIO)
            {
                pty->closed = true;
                return WLT_PTY_ERR_CLOSED;
            }
            return WLT_PTY_ERR_IO;
        }
        off += (size_t)n;
    }
    return WLT_PTY_OK;
}

wlt_pty_error_t
wlt_pty_resize(wlt_Pty *pty, uint32_t cols, uint32_t rows)
{
    if (pty == NULL || cols == 0 || rows == 0)
        return WLT_PTY_ERR_INVALID_ARG;
    if (pty->master_fd < 0)
        return WLT_PTY_ERR_CLOSED;

    pty->cols = cols;
    pty->rows = rows;
    apply_window_size(pty->master_fd, cols, rows);
    return WLT_PTY_OK;
}

bool
wlt_pty_child_exited(wlt_Pty *pty)
{
    if (pty == NULL || pty->child_pid <= 0)
        return true;

    pid_t r = waitpid(pty->child_pid, &pty->child_status, WNOHANG);
    if (r == pty->child_pid)
    {
        pty->child_pid = -1;
        return true;
    }
    return false;
}

int
wlt_pty_fd(const wlt_Pty *pty)
{
    if (pty == NULL) return -1;
    return pty->master_fd;
}

pid_t
wlt_pty_pid(const wlt_Pty *pty)
{
    if (pty == NULL) return -1;
    return pty->child_pid;
}

const char *
wlt_pty_error_str(wlt_pty_error_t err)
{
    switch (err)
    {
        case WLT_PTY_OK:               return "ok";
        case WLT_PTY_ERR_INVALID_ARG:  return "invalid argument";
        case WLT_PTY_ERR_OPENPTY:      return "openpty failed";
        case WLT_PTY_ERR_FORK:         return "fork failed";
        case WLT_PTY_ERR_EXEC:         return "exec failed";
        case WLT_PTY_ERR_IO:           return "i/o error";
        case WLT_PTY_ERR_CLOSED:       return "pty closed";
        case WLT_PTY_ERR_NOMEM:        return "out of memory";
        default:                       return "unknown";
    }
}
