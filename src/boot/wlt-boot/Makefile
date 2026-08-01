# WLT-Boot Makefile (C.1: real UEFI handoff build)
CC = gcc
LD = ld
OBJCOPY = objcopy

CFLAGS = -ffreestanding -fno-stack-protector -fno-stack-check \
         -fshort-wchar -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -DCONFIG_X86_64 -Iinclude -I. \
         -Wall -Wextra -std=gnu11 -O2 \
         -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-builtin

LDFLAGS = -nostdlib -static -pie --no-dynamic-linker -T linker.ld

SRCS = boot.c console.c memory.c loader.c verify.c config.c freestanding.c monocypher.c
OBJS = $(SRCS:.c=.o)

TARGET = wlt-boot.efi

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
