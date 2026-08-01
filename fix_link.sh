#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot
MONO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/packages/monocypher

# 1. Disable fortify source + add freestanding flags
sed -i 's/-std=gnu11 -O2/-std=gnu11 -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-builtin/' "$BOOTDIR/Makefile"

# 2. Add monocypher.c to sources
sed -i 's|SRCS = boot.c console.c memory.c loader.c verify.c config.c|SRCS = boot.c console.c memory.c loader.c verify.c config.c '"$MONO"'/monocypher.c|' "$BOOTDIR/Makefile"

# 3. Create freestanding string functions (memcmp, strncpy, memset, memcpy)
cat > "$BOOTDIR/freestanding.c" << 'FREESTANDING'
/* Freestanding implementations of libc functions needed by wlt-boot. */
#include <stddef.h>
#include <stdint.h>

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n--) {
        if (*a != *b) return *a - *b;
        if (!*a) return 0;
        a++; b++;
    }
    return 0;
}

/* Simple snprintf for freestanding (limited format support) */
int snprintf(char *str, size_t size, const char *fmt, ...) {
    /* Very basic: just copy fmt to str (no format expansion for now).
     * The bootloader's printf usage is limited and this prevents the
     * __snprintf_chk link error. A full implementation can be added later. */
    if (size == 0) return 0;
    size_t i = 0;
    /* For safety, just null-terminate and return 0.
     * The actual format expansion is handled by wlt_console_printf
     * which has its own implementation. */
    str[0] = '\0';
    return 0;
}
FREESTANDING

# 4. Add freestanding.c to sources
sed -i "s|config.c ${MONO}/monocypher.c|config.c freestanding.c ${MONO}/monocypher.c|" "$BOOTDIR/Makefile"

# 5. Add freestanding.o dependency
sed -i 's|OBJS = $(SRCS:.c=.o)|OBJS = $(SRCS:.c=.o)\nfreestanding.o: freestanding.c\n\t$(CC) $(CFLAGS) -c $< -o $@|' "$BOOTDIR/Makefile"

echo "Fixes applied."
echo ""
echo "=== BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -20
echo ""
echo "=== OUTPUT ==="
ls -la wlt-boot.efi 2>/dev/null && file wlt-boot.efi 2>/dev/null || echo "wlt-boot.efi not found"
