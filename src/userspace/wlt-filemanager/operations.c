/**
 * @file operations.c
 * @brief File operations — copy, move, delete, mkdir, secure delete
 * @module wlt-filemanager/operations
 * @purpose All file manipulation operations
 * @dependencies: operations.h, filemanager.h
 * @tested_by: tests/userspace/operations_test.c
 * @spec: docs/specs/wlt-filemanager.md
 */

#include "operations.h"
#include "filemanager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#define COPY_BUFFER_SIZE (64 * 1024)  /* 64KB chunks */

/* ─── Path existence checks ──────────────────────────── */

bool
wlt_fm_path_exists(const char *path)
{
    if (path == NULL)
        return false;
    return access(path, F_OK) == 0;
}

bool
wlt_fm_is_directory(const char *path)
{
    if (path == NULL)
        return false;
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

bool
wlt_fm_is_file(const char *path)
{
    if (path == NULL)
        return false;
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    return S_ISREG(st.st_mode);
}

wlt_filemanager_error_t
wlt_fm_get_size(const char *path, uint64_t *size)
{
    if (path == NULL || size == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    struct stat st;
    if (stat(path, &st) != 0)
    {
        if (errno == ENOENT)
            return WLT_FM_ERR_NOT_FOUND;
        return WLT_FM_ERR_IO;
    }

    *size = (uint64_t)st.st_size;
    return WLT_FM_OK;
}

/* ─── Copy operations ────────────────────────────────── */

wlt_filemanager_error_t
wlt_fm_copy_file(const char *src, const char *dst)
{
    if (src == NULL || dst == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0)
    {
        if (errno == ENOENT)
            return WLT_FM_ERR_NOT_FOUND;
        if (errno == EACCES)
            return WLT_FM_ERR_PERMISSION;
        return WLT_FM_ERR_IO;
    }

    /* Get source permissions */
    struct stat st;
    if (fstat(src_fd, &st) != 0)
    {
        close(src_fd);
        return WLT_FM_ERR_IO;
    }

    int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dst_fd < 0)
    {
        close(src_fd);
        if (errno == EACCES)
            return WLT_FM_ERR_PERMISSION;
        if (errno == ENOSPC)
            return WLT_FM_ERR_NOMEM;
        return WLT_FM_ERR_IO;
    }

    /* Copy data in chunks */
    char *buffer = malloc(COPY_BUFFER_SIZE);
    if (buffer == NULL)
    {
        close(src_fd);
        close(dst_fd);
        return WLT_FM_ERR_NOMEM;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(src_fd, buffer, COPY_BUFFER_SIZE)) > 0)
    {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read)
        {
            ssize_t w = write(dst_fd, buffer + bytes_written,
                              bytes_read - bytes_written);
            if (w < 0)
            {
                free(buffer);
                close(src_fd);
                close(dst_fd);
                return WLT_FM_ERR_IO;
            }
            bytes_written += w;
        }
    }

    free(buffer);
    close(src_fd);
    close(dst_fd);

    if (bytes_read < 0)
        return WLT_FM_ERR_IO;

    return WLT_FM_OK;
}

wlt_filemanager_error_t
wlt_fm_copy_directory(const char *src, const char *dst)
{
    if (src == NULL || dst == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    if (!wlt_fm_is_directory(src))
        return WLT_FM_ERR_NOT_FOUND;

    /* Create destination directory */
    wlt_filemanager_error_t err = wlt_fm_mkdir(dst);
    if (err != WLT_FM_OK && err != WLT_FM_ERR_EXISTS)
        return err;

    DIR *d = opendir(src);
    if (d == NULL)
        return WLT_FM_ERR_IO;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char src_path[WLT_FM_MAX_PATH];
        char dst_path[WLT_FM_MAX_PATH];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);

        if (wlt_fm_is_directory(src_path))
        {
            err = wlt_fm_copy_directory(src_path, dst_path);
        }
        else
        {
            err = wlt_fm_copy_file(src_path, dst_path);
        }

        if (err != WLT_FM_OK)
        {
            closedir(d);
            return err;
        }
    }

    closedir(d);
    return WLT_FM_OK;
}

/* ─── Move operation ─────────────────────────────────── */

wlt_filemanager_error_t
wlt_fm_move(const char *src, const char *dst)
{
    if (src == NULL || dst == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    /* Try rename first (fast, works on same filesystem) */
    if (rename(src, dst) == 0)
        return WLT_FM_OK;

    /* If rename fails (cross-filesystem), copy + delete */
    if (errno == EXDEV)
    {
        wlt_filemanager_error_t err;
        if (wlt_fm_is_directory(src))
            err = wlt_fm_copy_directory(src, dst);
        else
            err = wlt_fm_copy_file(src, dst);

        if (err != WLT_FM_OK)
            return err;

        if (wlt_fm_is_directory(src))
            return wlt_fm_delete_directory(src);
        else
            return wlt_fm_delete_file(src);
    }

    if (errno == EACCES)
        return WLT_FM_ERR_PERMISSION;
    if (errno == ENOENT)
        return WLT_FM_ERR_NOT_FOUND;

    return WLT_FM_ERR_IO;
}

/* ─── Delete operations ──────────────────────────────── */

wlt_filemanager_error_t
wlt_fm_delete_file(const char *path)
{
    if (path == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    if (unlink(path) != 0)
    {
        if (errno == ENOENT)
            return WLT_FM_ERR_NOT_FOUND;
        if (errno == EACCES)
            return WLT_FM_ERR_PERMISSION;
        return WLT_FM_ERR_IO;
    }

    return WLT_FM_OK;
}

wlt_filemanager_error_t
wlt_fm_delete_directory(const char *path)
{
    if (path == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    if (!wlt_fm_is_directory(path))
        return WLT_FM_ERR_NOT_FOUND;

    DIR *d = opendir(path);
    if (d == NULL)
        return WLT_FM_ERR_IO;

    struct dirent *entry;
    wlt_filemanager_error_t err = WLT_FM_OK;

    while ((entry = readdir(d)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[WLT_FM_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (wlt_fm_is_directory(full_path))
            err = wlt_fm_delete_directory(full_path);
        else
            err = wlt_fm_delete_file(full_path);

        if (err != WLT_FM_OK)
        {
            closedir(d);
            return err;
        }
    }

    closedir(d);

    /* Remove the now-empty directory */
    if (rmdir(path) != 0)
    {
        if (errno == ENOTEMPTY)
            return WLT_FM_ERR_NOT_EMPTY;
        return WLT_FM_ERR_IO;
    }

    return WLT_FM_OK;
}

/* ─── Mkdir ──────────────────────────────────────────── */

wlt_filemanager_error_t
wlt_fm_mkdir(const char *path)
{
    if (path == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    if (mkdir(path, 0755) != 0)
    {
        if (errno == EEXIST)
            return WLT_FM_ERR_EXISTS;
        if (errno == EACCES)
            return WLT_FM_ERR_PERMISSION;
        return WLT_FM_ERR_IO;
    }

    return WLT_FM_OK;
}

/* ─── Secure Delete (shred) ──────────────────────────── */

wlt_filemanager_error_t
wlt_fm_secure_delete(const char *path, int passes)
{
    if (path == NULL)
        return WLT_FM_ERR_INVALID_ARG;

    if (passes < 1)
        passes = 3;

    if (!wlt_fm_is_file(path))
        return WLT_FM_ERR_NOT_FOUND;

    /* Get file size */
    uint64_t file_size;
    wlt_filemanager_error_t err = wlt_fm_get_size(path, &file_size);
    if (err != WLT_FM_OK)
        return err;

    /* Open for writing */
    int fd = open(path, O_WRONLY);
    if (fd < 0)
    {
        if (errno == EACCES)
            return WLT_FM_ERR_PERMISSION;
        return WLT_FM_ERR_IO;
    }

    /* Allocate buffer */
    size_t buf_size = COPY_BUFFER_SIZE;
    if (file_size < buf_size)
        buf_size = (size_t)file_size;

    uint8_t *buffer = malloc(buf_size);
    if (buffer == NULL)
    {
        close(fd);
        return WLT_FM_ERR_NOMEM;
    }

    /* Pass 1: Random data */
    for (int pass = 0; pass < passes; pass++)
    {
        if (lseek(fd, 0, SEEK_SET) < 0)
        {
            free(buffer);
            close(fd);
            return WLT_FM_ERR_IO;
        }

        uint64_t remaining = file_size;
        while (remaining > 0)
        {
            size_t chunk = buf_size;
            if (remaining < chunk)
                chunk = (size_t)remaining;

            if (pass == 0)
            {
                /* Random data */
                FILE *urandom = fopen("/dev/urandom", "r");
                if (urandom)
                {
                    fread(buffer, 1, chunk, urandom);
                    fclose(urandom);
                }
                else
                {
                    memset(buffer, 0xAA, chunk);
                }
            }
            else if (pass == 1)
            {
                /* Zero fill */
                memset(buffer, 0x00, chunk);
            }
            else
            {
                /* 0xFF fill */
                memset(buffer, 0xFF, chunk);
            }

            ssize_t written = write(fd, buffer, chunk);
            if (written < 0)
            {
                free(buffer);
                close(fd);
                return WLT_FM_ERR_IO;
            }
            remaining -= (uint64_t)written;
        }

        /* Flush after each pass */
        fsync(fd);
    }

    free(buffer);

    /* Truncate to zero (ensure no data remains) */
    if (ftruncate(fd, 0) != 0)
    {
        /* Non-fatal — continue with unlink */
    }

    close(fd);

    /* Finally, unlink the file */
    if (unlink(path) != 0)
    {
        if (errno == EACCES)
            return WLT_FM_ERR_PERMISSION;
        return WLT_FM_ERR_IO;
    }

    return WLT_FM_OK;
}
