# WLT-Shell Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu11 -O2 -Iinclude

SRCS = shell.c taskbar.c launcher.c notifications.c tray.c app_launch.c ipc.c config.c
OBJS = $(SRCS:.c=.o)

TARGET = libwlt-shell.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

# Dependencies
shell.o: include/shell.h
taskbar.o: include/shell.h
launcher.o: include/shell.h
notifications.o: include/shell.h
tray.o: include/shell.h
app_launch.o: include/shell.h
ipc.o: include/shell.h
config.o: include/shell.h
