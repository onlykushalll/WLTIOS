# WLT-Settings build
# ==================
# Builds the settings backend (static library + objects). There is no
# main() here; the WLT-Compositor hosts this UI, so we ship a linkable
# library that the compositor and unit tests pull in.
#
# Spec: docs/specs/wlt-settings.md

CC      ?= cc
AR      ?= ar
PKGCONF ?= pkg-config

# WLT-Widgets / WLT-Compositor are pulled in by the host, not here, so we
# keep the build self-contained: only settings.h is needed.
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
           -fPIC -D_GNU_SOURCE \
           -Iinclude
LDFLAGS ?=
LDLIBS  ?=

# ----------------------------------------------------------------- #
# Sources / objects                                                 #
# ----------------------------------------------------------------- #
SRCS = settings.c \
       config_io.c \
       network_panel.c \
       display_panel.c \
       security_panel.c \
       privacy_panel.c

OBJS = $(SRCS:.c=.o)
HDRS = include/settings.h

LIB  = libwlt-settings.a

# ----------------------------------------------------------------- #
# Phony targets                                                     #
# ----------------------------------------------------------------- #
.PHONY: all clean check install

all: $(LIB)

# ----------------------------------------------------------------- #
# Static library                                                    #
# ----------------------------------------------------------------- #
$(LIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)

# ----------------------------------------------------------------- #
# Compilation rules                                                 #
# ----------------------------------------------------------------- #
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

# ----------------------------------------------------------------- #
# Install (system config dir + library + headers)                   #
# ----------------------------------------------------------------- #
PREFIX ?= /usr
install: all
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include/wlt
	install -d $(DESTDIR)/etc/wlt
	install -m 0644 $(LIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 0644 include/settings.h $(DESTDIR)$(PREFIX)/include/wlt/

# ----------------------------------------------------------------- #
# Sanity check: compile-only (no daemon needed)                     #
# ----------------------------------------------------------------- #
check: all
	@echo "wlt-settings: build OK ($(OBJS))"

# ----------------------------------------------------------------- #
# Clean                                                             #
# ----------------------------------------------------------------- #
clean:
	rm -f $(OBJS) $(LIB)
