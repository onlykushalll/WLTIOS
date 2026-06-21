# WLT-Init Makefile
# Builds the init system (PID 1)

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu11 -O2 -Iinclude \
         -static -nostdlib -ffreestanding
LDFLAGS = -static

SRCS = init.c service.c monitor.c dependency.c tor.c mount.c log.c
OBJS = $(SRCS:.c=.o)

TARGET = wlt-init

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -D $(TARGET) $(DESTDIR)/usr/bin/wlt-init

# Dependencies
init.o: include/init.h include/service.h include/monitor.h include/dependency.h include/tor.h include/mount.h include/log.h
service.o: include/service.h include/init.h include/log.h
monitor.o: include/monitor.h include/service.h include/init.h include/log.h
dependency.o: include/dependency.h include/init.h include/log.h
tor.o: include/tor.h include/init.h include/service.h include/log.h
mount.o: include/mount.h include/init.h include/log.h
log.o: include/log.h
