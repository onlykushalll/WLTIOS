# WLT-Permissions Makefile
#
# Builds the app permission system: manifest parser, permission manager,
# grant-state persistence, privacy badges, and the security audit log.
#
# Outputs:
#   libwlt-permissions.so   â shared library (linked by wlt-shell, wlt-init)
#   libwlt-permissions.a    â static library (for unit tests)
#
# The permission daemon binary (wlt-permissionsd, started by wlt-init via
# the wlt-permissions.service unit) is built from a separate main.c once
# the IPC contract with WLT-Shell stabilises; see TODO(wlt-permissions).
#
# Links against glibc for file/directory and time helpers. -D_GNU_SOURCE
# is required for gmtime_r() and O_CLOEXEC.

CC       = gcc
CFLAGS   = -Wall -Wextra -Werror -std=gnu11 -O2 -Iinclude -D_GNU_SOURCE
LDFLAGS  =

SHARED_CFLAGS  = -fPIC
SHARED_LDFLAGS = -fPIC -shared
STATIC_CFLAGS  = -fPIC

SRCS = permissions.c \
       manifest.c \
       persist.c \
       audit.c \
       badges.c

OBJS_SHARED = $(SRCS:.c=.shared.o)
OBJS_STATIC = $(SRCS:.c=.static.o)

LIB_SHARED  = libwlt-permissions.so
LIB_STATIC  = libwlt-permissions.a

HEADERS = include/permissions.h \
          include/manifest.h

PREFIX  ?= /usr
LIBDIR  ?= $(PREFIX)/lib
INCDIR  ?= $(PREFIX)/include

.PHONY: all clean install test

all: $(LIB_SHARED) $(LIB_STATIC)

# âââ Shared Library ââââââââââââââââââââââââââââââââââââ

$(LIB_SHARED): $(OBJS_SHARED)
	$(CC) $(SHARED_LDFLAGS) -o $@ $(OBJS_SHARED) $(LDFLAGS)

%.shared.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(SHARED_CFLAGS) -c $< -o $@

# âââ Static Library ââââââââââââââââââââââââââââââââââââ

$(LIB_STATIC): $(OBJS_STATIC)
	ar rcs $@ $(OBJS_STATIC)

%.static.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(STATIC_CFLAGS) -c $< -o $@

# âââ Install âââââââââââââââââââââââââââââââââââââââââââ

install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCDIR)/wlt-permissions
	install -m 0755 $(LIB_SHARED) $(DESTDIR)$(LIBDIR)/
	install -m 0644 $(LIB_STATIC) $(DESTDIR)$(LIBDIR)/
	install -m 0644 $(HEADERS) $(DESTDIR)$(INCDIR)/wlt-permissions/
	ldconfig || true

# âââ Test ââââââââââââââââââââââââââââââââââââââââââââââ
#
# Tests live in tests/userspace/ and link against the static library.
# They stub the on-disk paths (grants dir, audit log) with tmpdirs and
# validate the parsing/grant/badge logic without root.

test: $(LIB_STATIC)
	@echo "Run tests from tests/userspace/ (see tests/Makefile)"

# âââ Clean âââââââââââââââââââââââââââââââââââââââââââââ

clean:
	rm -f $(OBJS_SHARED) $(OBJS_STATIC) $(LIB_SHARED) $(LIB_STATIC)

# âââ Dependency Hints ââââââââââââââââââââââââââââââââââ
#
# Each .c file declares which headers it directly includes so that
# touching a header triggers a rebuild of every dependent object.

permissions.shared.o permissions.static.o: \
    include/permissions.h include/manifest.h

manifest.shared.o manifest.static.o: \
    include/manifest.h

persist.shared.o persist.static.o: \
    include/permissions.h include/manifest.h

audit.shared.o audit.static.o: \
    include/permissions.h include/manifest.h

badges.shared.o badges.static.o: \
    include/permissions.h include/manifest.h
