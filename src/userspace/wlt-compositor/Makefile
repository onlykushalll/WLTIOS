# WLT-Compositor Makefile

CC = gcc
# -DWLR_USE_UNSTABLE is required by wlroots 0.18 unstable API headers.
CFLAGS = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu11 -O2 -I. -Iinclude -DWLR_USE_UNSTABLE \
         $(shell pkg-config --cflags wlroots-0.18 wayland-server 2>/dev/null || echo "")
LDFLAGS = $(shell pkg-config --libs wlroots-0.18 wayland-server 2>/dev/null || echo "")

# Source files (fix: filenames are output.c/input.c/view.c, not comp_*.c).
SRCS = compositor.c output.c input.c view.c xdg.c layer.c
OBJS = $(SRCS:.c=.o)

TARGET = libwlt-compositor.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
