# WLT-Crypto Makefile
#
# Builds the encryption layer:
#   - libwlt-crypto.so   — shared library
#   - libwlt-crypto.a    — static library
#
# This build covers the 8 files in the core scope:
#   crypto.c, keyvault.c, file_crypto.c, secure_delete.c
#   include/crypto.h, include/keyvault.h, include/file_crypto.h
#
# libsodium is optional: when the dev headers are present (detected via
# pkg-config) the build uses sodium_memzero / randombytes_buf; otherwise
# a volatile-memset + /dev/urandom fallback is compiled in. This keeps
# the build self-contained on minimal hosts (per ADR-06, musl libc).
#
# Spec: docs/specs/wlt-crypto.md

CC       ?= gcc
CFLAGS   ?= -std=gnu11 -O2
WARN      = -Wall -Wextra -Werror
CPPFLAGS  = -Iinclude -D_GNU_SOURCE

# Auto-detect libsodium. Only the -dev headers are required at build
# time; the runtime lib is linked when present.
HAVE_SODIUM := $(shell pkg-config --exists libsodium 2>/dev/null && echo yes)
ifeq ($(HAVE_SODIUM),yes)
    CPPFLAGS += -DHAVE_SODIUM $(shell pkg-config --cflags libsodium)
    LDLIBS    = $(shell pkg-config --libs libsodium)
else
    LDLIBS    =
endif

# Source files in the 8-file core scope. luks.c and dm_crypt.c are not
# part of this build; the LUKS API dispatchers in crypto.c return
# WLT_CRYPTO_ERR_UNSUPPORTED until those modules are linked.
SRCS = crypto.c \
       keyvault.c \
       file_crypto.c \
       secure_delete.c

HEADERS = include/crypto.h \
          include/keyvault.h \
          include/file_crypto.h

OBJS_SHARED = $(SRCS:.c=.shared.o)
OBJS_STATIC = $(SRCS:.c=.static.o)

LIB_SHARED  = libwlt-crypto.so
LIB_STATIC  = libwlt-crypto.a

PREFIX  ?= /usr
LIBDIR  ?= $(PREFIX)/lib
INCDIR  ?= $(PREFIX)/include

.PHONY: all clean install test check

all: $(LIB_SHARED) $(LIB_STATIC)

# ─── Shared Library ────────────────────────────────────

$(LIB_SHARED): $(OBJS_SHARED)
	$(CC) -shared -fPIC -o $@ $(OBJS_SHARED) $(LDLIBS)

%.shared.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) -fPIC -c $< -o $@

# ─── Static Library ────────────────────────────────────

$(LIB_STATIC): $(OBJS_STATIC)
	ar rcs $@ $(OBJS_STATIC)

%.static.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) -fPIC -c $< -o $@

# ─── Install ───────────────────────────────────────────

install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCDIR)/wlt-crypto
	install -m 0755 $(LIB_SHARED) $(DESTDIR)$(LIBDIR)/
	install -m 0644 $(LIB_STATIC) $(DESTDIR)$(LIBDIR)/
	install -m 0644 $(HEADERS) $(DESTDIR)$(INCDIR)/wlt-crypto/
	-ldconfig 2>/dev/null || true

# ─── Test ──────────────────────────────────────────────
#
# Tests live in tests/userspace/. They link against the static
# library and stub out the Key Vault IPC for unit testing.

test: $(LIB_STATIC)
	@echo "Run tests from tests/userspace/ (see tests/Makefile)"

check: test

# ─── Clean ─────────────────────────────────────────────

clean:
	rm -f $(OBJS_SHARED) $(OBJS_STATIC) $(LIB_SHARED) $(LIB_STATIC)

# ─── Dependency Hints ──────────────────────────────────

crypto.shared.o:        include/crypto.h include/keyvault.h include/file_crypto.h
crypto.static.o:        include/crypto.h include/keyvault.h include/file_crypto.h
keyvault.shared.o:      include/crypto.h include/keyvault.h
keyvault.static.o:      include/crypto.h include/keyvault.h
file_crypto.shared.o:   include/crypto.h include/keyvault.h include/file_crypto.h
file_crypto.static.o:   include/crypto.h include/keyvault.h include/file_crypto.h
secure_delete.shared.o: include/crypto.h
secure_delete.static.o: include/crypto.h
