# SPDX-License-Identifier: GPL-2.0
#
# wlt-tor-mgr: build
#
# Builds the `wlttor` CLI/daemon and a static library. The transport default
# is TCP (for control-protocol testing); production links a seL4 IPC transport
# in place of wlt_tor_transport_tcp().
#
#   make            # build wlttor
#   make check      # syntax/compile sanity
#   make install    # install to $(DESTDIR)/usr/bin/wlttor

CC      ?= gcc
AR      ?= ar
CFLAGS  ?= -std=gnu11 -O2 -g -Wall -Wextra -D_GNU_SOURCE -Iinclude
LDFLAGS ?=
LDLIBS  ?= -lpthread

BIN     := wlttor
LIB     := libwlttor.a

SRCS    := tor_mgr.c control.c circuits.c identity.c bridges.c config.c
OBJS    := $(SRCS:.c=.o)
HDRS    := include/tor_mgr.h

DESTDIR ?=
PREFIX  ?= /usr

.PHONY: all clean install check

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(LIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(BIN)
	./$(BIN) help >/dev/null

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin/wlttor

clean:
	rm -f $(OBJS) $(BIN) $(LIB) $(BIN).d
