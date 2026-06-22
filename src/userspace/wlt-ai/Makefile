# ============================================================
# Makefile - WLT-AI AI Assistant Core
#
# Builds the WLT-AI userspace library and daemon for WLTIOS.
#
# Targets:
#   make           - Build libwltai.a (static library)
#   make wlt-ai    - Build the AI daemon binary
#   make test      - Build and run unit tests
#   make clean     - Remove build artifacts
#   make install   - Install to DESTDIR
#
# Copyright (c) WLTIOS Project. All rights reserved.
# ============================================================

# ---- Toolchain ----------------------------------------------------

CC       ?= cc
AR       ?= ar
CFLAGS   ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS  = -Iinclude -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS  ?=
LDLIBS    = -lcurl -lpthread

# ---- Source files -------------------------------------------------

SRC_DIR   = .
INC_DIR   = include
BUILD_DIR = build

LIB_SOURCES = ai.c tools.c memory.c vision.c config.c
LIB_OBJS    = $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIB_SOURCES))

DAEMON_SRC  = wlt_ai_daemon.c
DAEMON_BIN  = wlt-ai

LIB_NAME    = libwltai.a

# ---- Installation paths ------------------------------------------

PREFIX      ?= /usr
LIBDIR      ?= $(PREFIX)/lib
INCLUDEDIR  ?= $(PREFIX)/include
BINDIR      ?= $(PREFIX)/bin
DESTDIR     ?=

# ---- Rules --------------------------------------------------------

.PHONY: all clean test install

all: $(BUILD_DIR) $(LIB_NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Static library
$(LIB_NAME): $(LIB_OBJS)
	$(AR) rcs $@ $(LIB_OBJS)

# Compile each source file
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Daemon binary (if daemon source exists)
$(DAEMON_BIN): $(DAEMON_SRC) $(LIB_NAME)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L. -lwltai $(LDLIBS) -o $@

# ---- Tests --------------------------------------------------------

TEST_SRC  = test_ai.c
TEST_BIN  = $(BUILD_DIR)/test_ai

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(LIB_NAME) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TEST_SRC) -L. -lwltai $(LDLIBS) -o $@

# ---- Install ------------------------------------------------------

install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)/wlt-ai
	install -m 644 $(LIB_NAME) $(DESTDIR)$(LIBDIR)/
	install -m 644 $(INC_DIR)/ai.h $(DESTDIR)$(INCLUDEDIR)/wlt-ai/

# ---- Clean --------------------------------------------------------

clean:
	rm -rf $(BUILD_DIR) $(LIB_NAME) $(DAEMON_BIN) $(TEST_BIN)

# ---- Dependencies -------------------------------------------------

# Header dependency: all objects depend on the public header
$(LIB_OBJS): $(INC_DIR)/ai.h
