# WLT-Update Makefile
#
# Builds the atomic A/B update agent:
#   - libwlt-update.so   â shared library
#   - libwlt-update.a    â static library
#
# Source files in scope:
#   update.c, partition.c, verify.c, config.c
#   include/update.h, include/partition.h
#
# libsodium is REQUIRED for Ed25519 verification. It is detected via
# pkg-config; when absent the build still compiles (verify.c returns
# WLT_UPDATE_ERR_UNSUPPORTED from every entry point) so a minimal host
# can link the library â but signature checks will refuse to run, which
# is the safe failure mode per spec.
#
# The `wltupdate` CLI binary is intentionally NOT built here: it lives in
# a separate main.c that links against this library (see tests/Makefile).
#
# Spec: docs/specs/wlt-update.md

CC       ?= gcc
CFLAGS   ?= -std=gnu11 -O2
WARN      = -Wall -Wextra -Werror
CPPFLAGS  = -Iinclude -D_GNU_SOURCE

# Auto-detect libsodium. Only the -dev headers are needed at build time.
HAVE_SODIUM := $(shell pkg-config --exists libsodium 2>/dev/null && echo yes)
ifeq ($(HAVE_SODIUM),yes)
    CPPFLAGS += -DHAVE_SODIUM $(shell pkg-config --cflags libsodium)
    LDLIBS    = $(shell pkg-config --libs libsodium)
else
    LDLIBS    =
endif

SRCS = update.c \
       partition.c \
       verify.c \
       config.c \
       rollback.c

HEADERS = include/update.h \
          include/partition.h

OBJS_SHARED = $(SRCS:.c=.shared.o)
OBJS_STATIC = $(SRCS:.c=.static.o)

LIB_SHARED  = libwlt-update.so
LIB_STATIC  = libwlt-update.a

PREFIX  ?= /usr
LIBDIR  ?= $(PREFIX)/lib
INCDIR  ?= $(PREFIX)/include

.PHONY: all clean install test check

all: $(LIB_SHARED) $(LIB_STATIC)

# âââ Shared library ââââââââââââââââââââââââââââââââââââ

$(LIB_SHARED): $(OBJS_SHARED)
	$(CC) -shared -fPIC -o $@ $(OBJS_SHARED) $(LDLIBS)

%.shared.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) -fPIC -c $< -o $@

# âââ Static library ââââââââââââââââââââââââââââââââââââ

$(LIB_STATIC): $(OBJS_STATIC)
	ar rcs $@ $(OBJS_STATIC)

%.static.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) -fPIC -c $< -o $@

# âââ Install âââââââââââââââââââââââââââââââââââââââââââ

install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCDIR)/wlt-update
	install -m 0755 $(LIB_SHARED) $(DESTDIR)$(LIBDIR)/
	install -m 0644 $(LIB_STATIC) $(DESTDIR)$(LIBDIR)/
	install -m 0644 $(HEADERS) $(DESTDIR)$(INCDIR)/wlt-update/
	-ldconfig 2>/dev/null || true

# âââ Test ââââââââââââââââââââââââââââââââââââââââââââââ
#
# Tests live in tests/userspace/ and link against the static library.
# A stub Tor SOCKS server and a fake boot config are used so tests run
# without root or network.

test: $(LIB_STATIC)
	@echo "Run tests from tests/userspace/ (see tests/Makefile)"

check: test

# âââ Clean âââââââââââââââââââââââââââââââââââââââââââââ

clean:
	rm -f $(OBJS_SHARED) $(OBJS_STATIC) $(LIB_SHARED) $(LIB_STATIC)

# âââ Dependency hints ââââââââââââââââââââââââââââââââââ

update.shared.o:        include/update.h include/partition.h
update.static.o:        include/update.h include/partition.h
partition.shared.o:     include/update.h include/partition.h
partition.static.o:     include/update.h include/partition.h
verify.shared.o:        include/update.h
verify.static.o:        include/update.h
config.shared.o:        include/update.h
config.static.o:        include/update.h
