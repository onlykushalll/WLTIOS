# WLT-Terminal Makefile
# Builds the Wayland terminal client (wlt-term-wayland.c)
#
# Spec: docs/specs/wlt-terminal.md
# ADR:  docs/adr/05-wayland-wlroots.md

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -O2 -D_GNU_SOURCE \
         -Wno-unused-parameter -Wno-unused-function -Wno-misleading-indentation
WAYLAND_CFLAGS = $(shell pkg-config --cflags wayland-client 2>/dev/null)
WAYLAND_LIBS = $(shell pkg-config --libs wayland-client 2>/dev/null) -lutil

# The old terminal library (terminal.c etc.) is retained as the spec'd
# library implementation. The actual running binary is wlt-term-wayland.c.
LIB_SRCS = terminal.c pty.c ansi.c render.c scrollback.c config.c
LIB_OBJS = $(LIB_SRCS:.c=.o)
LIB  = libwlt-terminal.a

# Wayland terminal binary (the one that actually boots)
TERM = wlt-terminal

.PHONY: all clean check install

all: $(LIB) $(TERM)

$(LIB): $(LIB_OBJS)
	ar rcs $@ $(OBJS)

$(TERM): wlt-term-wayland.c xdg-shell-client-protocol.c xdg-shell-client-protocol.h
	$(CC) $(CFLAGS) $(WAYLAND_CFLAGS) -I . \
		wlt-term-wayland.c xdg-shell-client-protocol.c \
		$(WAYLAND_LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

check: $(LIB)
	@echo "wlt-terminal: archive built at $(LIB)"

install: $(TERM)
	install -D -m 755 $(TERM) $(DESTDIR)/usr/bin/wlt-terminal

clean:
	rm -f $(OBJS) $(LIB) $(TERM)

# Header dependencies
terminal.o: include/terminal.h include/pty.h include/ansi.h
pty.o:     include/pty.h include/terminal.h
ansi.o:    include/ansi.h include/terminal.h
render.o:  include/terminal.h
scrollback.o: include/terminal.h
config.o:  include/terminal.h
