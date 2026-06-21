# WLT-Terminal Makefile
# Builds the terminal emulator static library
#
# Spec: docs/specs/wlt-terminal.md
# ADR:  docs/adr/05-wayland-wlroots.md

CC = gcc
# -iquoteinclude keeps "..." searches local without polluting <...>
CFLAGS = -Wall -Wextra -std=gnu11 -O2 -iquoteinclude \
         -D_GNU_SOURCE \
         -Wno-unused-parameter \
         -Wno-unused-function
LDFLAGS = -lutil

SRCS = terminal.c pty.c ansi.c render.c scrollback.c config.c
OBJS = $(SRCS:.c=.o)
LIB  = libwlt-terminal.a

.PHONY: all clean install check

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

check: $(LIB)
	@echo "wlt-terminal: archive built at $(LIB)"

install: $(LIB)
	install -D -m 644 $(LIB) $(DESTDIR)/usr/lib/$(LIB)
	install -d $(DESTDIR)/usr/include/wlt-terminal
	install -D -m 644 include/*.h $(DESTDIR)/usr/include/wlt-terminal/

clean:
	rm -f $(OBJS) $(LIB)

# Header dependencies
terminal.o: include/terminal.h include/pty.h include/ansi.h
pty.o:     include/pty.h include/terminal.h
ansi.o:    include/ansi.h include/terminal.h
render.o:  include/terminal.h
scrollback.o: include/terminal.h
config.o:  include/terminal.h
