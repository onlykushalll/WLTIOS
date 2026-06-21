# WLT-Boot Makefile
# Builds the UEFI bootloader
#
# This is a skeleton — will be filled in when we have the cross-compiler

# Cross-compiler (x86_64-elf-gcc or clang)
CC = x86_64-elf-gcc
LD = x86_64-elf-ld
OBJCOPY = x86_64-elf-objcopy

# Flags for UEFI application
CFLAGS = -ffreestanding -fno-stack-protector -fno-stack-check \
         -fshort-wchar -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -DCONFIG_X86_64 -Iinclude -Wall -Wextra -Werror \
         -std=gnu11 -O2

LDFLAGS = -nostdlib -static -pie --no-dynamic-linker \
          -T linker.ld

# Source files
SRCS = boot.c console.c memory.c loader.c verify.c config.c
OBJS = $(SRCS:.c=.o)

# Output
TARGET = wlt-boot.efi

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

# Dependencies
boot.o: include/boot.h include/console.h include/memory.h include/loader.h include/verify.h include/config.h
console.o: include/console.h
memory.o: include/memory.h include/console.h
loader.o: include/loader.h include/console.h include/memory.h
verify.o: include/verify.h include/console.h
config.o: include/config.h include/console.h
