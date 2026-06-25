/**
 * @file wayland_test.c
 * @brief Minimal Wayland client — creates a red surface and commits it
 * @module wlt-terminal
 * @purpose Prove Wayland client-server communication works in QEMU guest
 *
 * Connects to $WAYLAND_DISPLAY, gets wl_compositor + wl_shm from registry,
 * creates a 200x200 red surface, commits it, runs for 3 seconds, exits.
 *
 * Build: gcc -O2 $(pkg-config --cflags wayland-client) wayland_test.c \
 *          $(pkg-config --libs wayland-client) -o wayland_test
 * Run:   WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run wayland_test
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>

struct wl_display *display = NULL;
struct wl_compositor *compositor = NULL;
struct wl_shm *shm = NULL;
struct wl_surface *surface = NULL;

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version)
{
    (void)data; (void)version;
    if (strcmp(interface, "wl_compositor") == 0)
    {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
        fprintf(stderr, "[wayland_test] Got wl_compositor\n");
    }
    else if (strcmp(interface, "wl_shm") == 0)
    {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
        fprintf(stderr, "[wayland_test] Got wl_shm\n");
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/* Create a shared memory buffer filled with red pixels */
static struct wl_buffer *create_red_buffer(int width, int height)
{
    int stride = width * 4;  /* 32bpp */
    int size = stride * height;
    char template[] = "/run/wayland-shm-XXXXXX";
    int fd = mkostemp(template, O_CLOEXEC);
    if (fd < 0)
    {
        fprintf(stderr, "[wayland_test] mkostemp failed\n");
        return NULL;
    }
    unlink(template);

    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        return NULL;
    }

    uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED)
    {
        close(fd);
        return NULL;
    }

    /* Fill with red (0xFFFF0000 in BGRA format) */
    for (int i = 0; i < width * height; i++)
    {
        pixels[i] = 0xFFFF0000;  /* BGRA: B=00, G=00, R=FF, A=FF */
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);

    munmap(pixels, size);
    close(fd);
    return buffer;
}

int main(void)
{
    fprintf(stderr, "[wayland_test] Starting\n");

    display = wl_display_connect(NULL);
    if (!display)
    {
        fprintf(stderr, "[wayland_test] ERROR: Cannot connect to Wayland display\n");
        fprintf(stderr, "[wayland_test] Make sure WAYLAND_DISPLAY and XDG_RUNTIME_DIR are set\n");
        return 1;
    }
    fprintf(stderr, "[wayland_test] Connected to Wayland display\n");

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !shm)
    {
        fprintf(stderr, "[wayland_test] ERROR: Missing wl_compositor or wl_shm\n");
        wl_display_disconnect(display);
        return 1;
    }

    /* Create a 200x200 surface */
    surface = wl_compositor_create_surface(compositor);
    if (!surface)
    {
        fprintf(stderr, "[wayland_test] ERROR: Cannot create surface\n");
        wl_display_disconnect(display);
        return 1;
    }
    fprintf(stderr, "[wayland_test] Surface created (200x200)\n");

    /* Create red buffer and attach */
    struct wl_buffer *buffer = create_red_buffer(200, 200);
    if (!buffer)
    {
        fprintf(stderr, "[wayland_test] ERROR: Cannot create buffer\n");
        wl_display_disconnect(display);
        return 1;
    }

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
    fprintf(stderr, "[wayland_test] Surface committed (red 200x200)\n");

    /* Run for 3 seconds to let the compositor display it */
    fprintf(stderr, "[wayland_test] Running for 3 seconds...\n");
    for (int i = 0; i < 30; i++)
    {
        wl_display_dispatch_pending(display);
        usleep(100000);  /* 100ms */
    }

    fprintf(stderr, "[wayland_test] Cleaning up\n");
    wl_buffer_destroy(buffer);
    wl_surface_destroy(surface);
    wl_display_disconnect(display);

    fprintf(stderr, "[wayland_test] SUCCESS — Wayland surface committed\n");
    printf("Wayland surface committed\n");
    return 0;
}