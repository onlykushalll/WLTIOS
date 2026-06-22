# WLT-FileManager Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu11 -O2 -Iinclude

SRCS = filemanager.c operations.c config.c
OBJS = $(SRCS:.c=.o)

TARGET = libwlt-filemanager.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

# Dependencies
filemanager.o: include/filemanager.h include/operations.h
operations.o: include/operations.h include/filemanager.h
config.o: include/filemanager.h
