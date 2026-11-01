# WLT-FileManager Makefile
#
# [fix 2026-08-16] Added the missing binary link step. filemanager.c is a
# complete, keyboard-wired Wayland file manager (real main(), real event
# loop, real wl_seat binding, real navigation) - this Makefile only ever
# archived its objects into libwlt-filemanager.a and never linked a
# runnable program, so the file manager has never actually shipped.
# Unlike wlt-settings/wlt-shell/wlt-terminal, there's no competing
# simpler reimplementation here - nothing was shipping in its place, so
# this is a pure addition, not a choice between two implementations.
# See docs/STATUS_ROADMAP.md for the fuller writeup. NOT build-verified
# by Claude (no compiler access on this side) - GLM should confirm this
# actually links and runs before it's trusted.

CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wno-format-truncation -Wno-stringop-truncation -Wno-type-limits -Wno-misleading-indentation -Wno-unused-result -std=gnu11 -O2 -Iinclude
WAYLAND_CFLAGS = $(shell pkg-config --cflags wayland-client 2>/dev/null)
WAYLAND_LIBS = $(shell pkg-config --libs wayland-client 2>/dev/null)

SRCS = filemanager.c operations.c config.c file_dnd.c
OBJS = $(SRCS:.c=.o)

TARGET = libwlt-filemanager.a
BIN = wlt-filemanager
RUST_DIR = wlt_fm_safe
RUST_LIB = $(RUST_DIR)/target/release/libwlt_fm_safe.a

.PHONY: all clean rust

all: $(TARGET) $(BIN)

rust:
	(cd $(RUST_DIR) && cargo build --release)

$(TARGET): $(OBJS)
	ar rcs $@ $(OBJS)

$(BIN): $(OBJS) xdg-shell-client-protocol.c xdg-shell-client-protocol.h rust
	$(CC) $(CFLAGS) $(WAYLAND_CFLAGS) -I . $(OBJS) \
		xdg-shell-client-protocol.c $(WAYLAND_LIBS) $(RUST_LIB) -o $@ \
		-lpthread -ldl -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(BIN)

# Dependencies
filemanager.o: include/filemanager.h include/operations.h
operations.o: include/operations.h include/filemanager.h
config.o: include/filemanager.h
