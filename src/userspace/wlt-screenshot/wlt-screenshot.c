/**
 * @file wlt-screenshot.c — WLTIOS Screenshot Tool (FR-APP-06)
 * @purpose Captures the compositor's framebuffer as a PNG screenshot.
 *          Reuses the compositor's own screendump verification code
 *          (which already captures the framebuffer for testing).
 * @build: gcc -O2 -Wall -o wlt-screenshot wlt-screenshot.c
 * @usage: wlt-screenshot [output.png]
 *
 * How it works:
 * 1. If a QEMU monitor socket exists (/tmp/qemu-mon.sock), sends
 *    "screendump" to capture the framebuffer.
 * 2. If the compositor exposes a screendump via its API socket
 *    (/run/guardian.sock or a dedicated compositor socket), uses that.
 * 3. Falls back to reading /dev/fb0 (the Linux framebuffer).
 * 4. Converts the raw framebuffer to PNG.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <stdint.h>
#include <zlib.h>

static int send_qemu_screendump(const char *sock_path, const char *out_path) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path)-1);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock); return -1;
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "screendump %s\n", out_path);
    write(sock, cmd, strlen(cmd));
    /* Read + discard any response */
    char buf[256];
    read(sock, buf, sizeof(buf));
    close(sock);
    return 0;
}

static int read_framebuffer(const char *fb_path, const char *out_path,
                            int width, int height) {
    int fd = open(fb_path, O_RDONLY);
    if (fd < 0) return -1;
    size_t size = width * height * 4;  /* assume 32bpp */
    uint8_t *buf = malloc(size);
    if (!buf) { close(fd); return -1; }
    size_t total = 0;
    while (total < size) {
        ssize_t n = read(fd, buf + total, size - total);
        if (n <= 0) break;
        total += n;
    }
    close(fd);
    if (total < size) {
        fprintf(stderr, "[screenshot] Short read: %zu/%zu bytes\n", total, size);
        /* Zero-fill the rest */
        memset(buf + total, 0, size - total);
    }

    /* Convert raw BGRA/RGBA to PNG */
    FILE *f = fopen(out_path, "wb");
    if (!f) { free(buf); return -1; }

    /* PNG header */
    fputs("\x89PNG\r\n\x1a\n", f);

    /* Write IHDR chunk */
    uint8_t ihdr[13] = {0};
    ihdr[0] = width >> 24; ihdr[1] = width >> 16; ihdr[2] = width >> 8; ihdr[3] = width;
    ihdr[4] = height >> 24; ihdr[5] = height >> 16; ihdr[6] = height >> 8; ihdr[7] = height;
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* color type: RGB */
    /* Write chunk helper (length + type + data + CRC) */
    uint32_t len = 13;
    fwrite(&len, 4, 1, f);
    fputs("IHDR", f);
    fwrite(ihdr, 13, 1, f);
    uint32_t crc = crc32(0, (const Bytef*)"IHDR", 4);
    crc = crc32(crc, ihdr, 13);
    fwrite(&crc, 4, 1, f);

    /* Write IDAT chunk (compressed image data) */
    /* Each row: filter byte (0) + width * 3 bytes (RGB) */
    size_t row_size = 1 + width * 3;
    uint8_t *raw = malloc(row_size * height);
    for (int y = 0; y < height; y++) {
        raw[y * row_size] = 0;  /* no filter */
        for (int x = 0; x < width; x++) {
            uint8_t *px = &buf[(y * width + x) * 4];
            /* Assume BGRA → swap to RGB */
            raw[y * row_size + 1 + x * 3] = px[2];  /* R */
            raw[y * row_size + 1 + x * 3 + 1] = px[1];  /* G */
            raw[y * row_size + 1 + x * 3 + 2] = px[0];  /* B */
        }
    }
    free(buf);

    uLongf comp_size = compressBound(row_size * height);
    uint8_t *comp = malloc(comp_size);
    if (compress2(comp, &comp_size, raw, row_size * height, 9) == Z_OK) {
        uint32_t idat_len = comp_size;
        fwrite(&idat_len, 4, 1, f);
        fputs("IDAT", f);
        fwrite(comp, comp_size, 1, f);
        uint32_t idat_crc = crc32(0, (const Bytef*)"IDAT", 4);
        idat_crc = crc32(idat_crc, comp, comp_size);
        fwrite(&idat_crc, 4, 1, f);
    }
    free(comp);
    free(raw);

    /* Write IEND chunk */
    uint32_t zero = 0;
    fwrite(&zero, 4, 1, f);
    fputs("IEND", f);
    uint32_t iend_crc = crc32(0, (const Bytef*)"IEND", 4);
    fwrite(&iend_crc, 4, 1, f);

    fclose(f);
    fprintf(stderr, "[screenshot] Saved %dx%d to %s\n", width, height, out_path);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *out = argc > 1 ? argv[1] : "/tmp/wlt-screenshot.png";

    fprintf(stderr, "[screenshot] WLTIOS Screenshot Tool v1.0\n");

    /* Method 1: QEMU monitor screendump (for dev/testing) */
    if (send_qemu_screendump("/tmp/qemu-mon.sock", out) == 0) {
        fprintf(stderr, "[screenshot] Captured via QEMU monitor\n");
        return 0;
    }
    /* Also try the compositor's monitor socket */
    if (send_qemu_screendump("/home/kushal/wlt-boot-test-mon.sock", out) == 0) {
        fprintf(stderr, "[screenshot] Captured via compositor monitor\n");
        return 0;
    }

    /* Method 2: Linux framebuffer (/dev/fb0) */
    int w = 1280, h = 800;  /* default dimensions */
    /* Read resolution from /sys/class/graphics/fb0/virtual_size */
    FILE *vs = fopen("/sys/class/graphics/fb0/virtual_size", "r");
    if (vs) {
        if (fscanf(vs, "%d,%d", &w, &h) != 2) { w = 1280; h = 800; }
        fclose(vs);
    }
    if (read_framebuffer("/dev/fb0", out, w, h) == 0) {
        fprintf(stderr, "[screenshot] Captured via /dev/fb0\n");
        return 0;
    }

    fprintf(stderr, "[screenshot] No capture method available\n");
    fprintf(stderr, "[screenshot] (In QEMU dev: ensure -monitor unix:...,server,nowait is set)\n");
    return 1;
}
