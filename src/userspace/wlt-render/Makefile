# WLT-Render Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu11 -O2 -Iinclude
LDFLAGS =

SRCS = render.c
OBJS = $(SRCS:.c=.o)

TARGET = libwlt-render.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
