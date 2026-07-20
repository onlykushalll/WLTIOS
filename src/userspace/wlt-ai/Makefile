# ============================================================
# Makefile - WLT-AI AI Assistant Core
#
# Builds libwlt-ai.a — the WLTIOS in-process AI library.
#
# No external HTTP dependency: ai.c uses wlt_http.c (raw socket)
# for the local LLM path, and curl_compat.h stubs for the
# cloud/STT/TTS paths (compile-only, no runtime curl).
#
# Targets:
#   make           - Build libwlt-ai.a (static library)
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

# ---- Source files -------------------------------------------------

SRC_DIR   = .
INC_DIR   = include
BUILD_DIR = build

# wlt_http.c replaces libcurl for the local LLM POST (loopback only).
# curl_compat.h stubs the remaining curl refs in ai.c (cloud/STT/TTS).
LIB_SOURCES = ai.c wlt_http.c tools.c memory.c config.c file_search.c vision.c
LIB_OBJS    = $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIB_SOURCES))

LIB_NAME    = libwlt-ai.a

# ---- Installation paths ------------------------------------------

PREFIX      ?= /usr
LIBDIR      ?= $(PREFIX)/lib
INCLUDEDIR  ?= $(PREFIX)/include/wlt-ai
DESTDIR     ?=

# ---- Rules --------------------------------------------------------

.PHONY: all clean install

all: $(BUILD_DIR) $(LIB_NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Static library
$(LIB_NAME): $(LIB_OBJS)
	$(AR) rcs $@ $(LIB_OBJS)

# Compile each source file
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# ---- Install ------------------------------------------------------

install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(LIB_NAME) $(DESTDIR)$(LIBDIR)/
	install -m 644 $(INC_DIR)/ai.h $(DESTDIR)$(INCLUDEDIR)/
	install -m 644 $(INC_DIR)/wlt_http.h $(DESTDIR)$(INCLUDEDIR)/

# ---- Clean --------------------------------------------------------

clean:
	rm -rf $(BUILD_DIR) $(LIB_NAME)

# ---- Dependencies -------------------------------------------------

# Header dependency: all objects depend on the public headers
$(LIB_OBJS): $(INC_DIR)/ai.h $(INC_DIR)/wlt_http.h $(INC_DIR)/curl_compat.h
