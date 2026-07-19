# WLT-Sandbox Makefile
#
# Builds the app sandboxing layer: cgroup v2, namespaces, seccomp-BPF,
# app launcher, monitor, and audit log.
#
# Outputs:
#   libwlt-sandbox.so   — shared library (linked by wlt-shell, etc.)
#   libwlt-sandbox.a    — static library (for unit tests)
#
# The daemon binary (wlt-sandboxd, started by wlt-init via the
# wlt-sandbox.service unit) is built from a separate main.c once the
# IPC contract with WLT-Shell stabilises; see TODO(wlt-sandbox) below.
#
# Links against glibc for syscall wrappers (mount, unshare, fork).
#_GNU_SOURCE is required for unshare(), clone3(), and pipe2().

CC       = gcc
CFLAGS   = -Wall -Wextra -Werror -std=gnu11 -O2 -Iinclude -I../wlt-permissions/include -D_GNU_SOURCE
LDFLAGS  =

SHARED_CFLAGS  = -fPIC
SHARED_LDFLAGS = -fPIC -shared
STATIC_CFLAGS  = -fPIC

SRCS = sandbox.c \
       manifest.c \
       launch.c \
       monitor.c \
       audit.c

OBJS_SHARED = $(SRCS:.c=.shared.o)
OBJS_STATIC = $(SRCS:.c=.static.o)

LIB_SHARED  = libwlt-sandbox.so
LIB_STATIC  = libwlt-sandbox.a

HEADERS = include/sandbox.h \
          include/manifest.h

PREFIX  ?= /usr
LIBDIR  ?= $(PREFIX)/lib
INCDIR  ?= $(PREFIX)/include

.PHONY: all clean install test

all: $(LIB_SHARED) $(LIB_STATIC)

# ─── Shared Library ────────────────────────────────────

$(LIB_SHARED): $(OBJS_SHARED)
	$(CC) $(SHARED_LDFLAGS) -o $@ $(OBJS_SHARED) $(LDFLAGS)

%.shared.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(SHARED_CFLAGS) -c $< -o $@

# ─── Static Library ────────────────────────────────────

$(LIB_STATIC): $(OBJS_STATIC)
	ar rcs $@ $(OBJS_STATIC)

%.static.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(STATIC_CFLAGS) -c $< -o $@

# ─── Install ───────────────────────────────────────────

install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCDIR)/wlt-sandbox
	install -m 0755 $(LIB_SHARED) $(DESTDIR)$(LIBDIR)/
	install -m 0644 $(LIB_STATIC) $(DESTDIR)$(LIBDIR)/
	install -m 0644 $(HEADERS) $(DESTDIR)$(INCDIR)/wlt-sandbox/
	ldconfig || true

# ─── Test ──────────────────────────────────────────────
#
# Tests live in tests/userspace/ and link against the static library.
# The sandbox setup functions require root + cgroups v2, so unit tests
# stub out the kernel interactions and validate the parsing/validation
# logic only.

test: $(LIB_STATIC)
	@echo "Run tests from tests/userspace/ (see tests/Makefile)"

# ─── Clean ─────────────────────────────────────────────

clean:
	rm -f $(OBJS_SHARED) $(OBJS_STATIC) $(LIB_SHARED) $(LIB_STATIC)

# ─── Dependency Hints ──────────────────────────────────
#
# Each .c file declares which headers it directly includes so that
# touching a header triggers a rebuild of every dependent object.

sandbox.shared.o sandbox.static.o: \
    include/sandbox.h include/manifest.h

manifest.shared.o manifest.static.o: \
    include/manifest.h

launch.shared.o launch.static.o: \
    include/sandbox.h include/manifest.h

monitor.shared.o monitor.static.o: \
    include/sandbox.h include/manifest.h

audit.shared.o audit.static.o: \
    include/sandbox.h
