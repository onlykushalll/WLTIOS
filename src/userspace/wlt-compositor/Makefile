# WLT-Compositor Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu11 -O2 -Iinclude \
         $(shell pkg-config --cflags wlroots-0.18 wayland-server 2>/dev/null || echo "")
LDFLAGS = $(shell pkg-config --libs wlroots-0.18 wayland-server 2>/dev/null || echo "")

SRCS = compositor.c comp_output.c comp_input.c comp_view.c xdg.c layer.c
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
