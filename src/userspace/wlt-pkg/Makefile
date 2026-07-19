# WLT-Pkg Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wno-format-truncation -Wno-stringop-truncation -std=gnu11 -O2 -Iinclude

SRCS = pkg.c store.c transaction.c download.c verify.c dependency.c
OBJS = $(SRCS:.c=.o)

TARGET = libwlt-pkg.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

# Dependencies
pkg.o: include/pkg.h
store.o: include/pkg.h
transaction.o: include/pkg.h
download.o: include/pkg.h
verify.o: include/pkg.h
dependency.o: include/pkg.h
