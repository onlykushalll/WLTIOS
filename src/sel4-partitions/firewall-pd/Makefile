# SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause
#
# WLTIOS - seL4 Firewall Protection Domain
# Makefile
#
# Builds firewall.elf: a bare-metal seL4 Microkit protection domain.
#
# This Makefile is invoked by the top-level seL4 build system (which
# supplies MICROKIT_SDK, MICROKIT_CONFIG, CROSS_COMPILE and the sDDF /
# Microkit include + lib paths). It can also be used standalone for a
# syntax/compile check by setting CROSS_COMPILE and pointing the
# include paths at a Microkit SDK install.
#
# The PD has NO libc, NO sockets, NO filesystem. We link against the
# Microkit runtime (libmicrokit) and the sDDF net helpers only.

# ---- Toolchain ------------------------------------------------------
CROSS_COMPILE ?= aarch64-linux-gnu-
CC      := $(CROSS_COMPILE)gcc
AR      := $(CROSS_COMPILE)ar
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE    := $(CROSS)size

# ---- Build configuration -------------------------------------------
# MICROKIT_SDK  : path to an unpacked Microkit SDK release
# MICROKIT_CONFIG : board+config name, e.g. imx8mm_evk/release
MICROKIT_SDK    ?= $(MICROKIT_SDK_DIR)
MICROKIT_CONFIG ?= qemu_virt_aarch64/release

SDK_BOARD   := $(firstword $(subst /, ,$(MICROKIT_CONFIG)))
SDK_BUILD   := $(lastword $(subst /, ,$(MICROKIT_CONFIG)))
SDK_DIR     := $(MICROKIT_SDK)/board/$(SDK_BOARD)/$(SDK_BUILD)

# Include + library search paths.
MICROKIT_INC := $(SDK_DIR)/include
MICROKIT_LIB := $(SDK_DIR)/lib
SDDF_INC     ?= $(MICROKIT_SDK)/sddf/include

# ---- PD identity ----------------------------------------------------
PD_NAME   := firewall
BUILD_DIR := build
SRC_DIR   := .

# ---- Sources --------------------------------------------------------
SRCS_C := \
	$(SRC_DIR)/firewall.c    \
	$(SRC_DIR)/rules.c       \
	$(SRC_DIR)/kill_switch.c \
	$(SRC_DIR)/packet.c

OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS_C))

# ---- Flags ----------------------------------------------------------
# Bare-metal: no libc, no start files, no PIC, no FP, freestanding.
PD_CFLAGS := \
	-ffreestanding -nostdlib -nostdinc \
	-fno-pic -fno-pie -fno-stack-protector \
	-fno-common -fno-asynchronous-unwind-tables \
	-mgeneral-regs-only -mstrict-align \
	-std=c11 -Wall -Wextra -Werror -Wno-unused-parameter \
	-O2 -g3 \
	-I$(SRC_DIR)/include \
	-I$(MICROKIT_INC) \
	-I$(SDDF_INC) \
	-DFIREWALL_PD=1

# The rule table lives in .rodata so Linux cannot reach it.
PD_LDFLAGS := \
	-nostdlib -nostartfiles -static \
	-Wl,--build-id=none \
	-Wl,-z,noexecstack \
	-Wl,--gc-sections \
	-L$(MICROKIT_LIB)

# Microkit provides the PD entry trampoline + the syscall stubs.
PD_LDLIBS := -lmicrokit

# Optional debug logging (hand-rolled printf over microkit_dbg_putc).
ifeq ($(FW_DEBUG),1)
PD_CFLAGS += -DFW_DEBUG
endif

# ---- Targets --------------------------------------------------------
.PHONY: all clean syntax-check

all: $(BUILD_DIR)/$(PD_NAME).elf

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(PD_CFLAGS) -c $< -o $@

# The linker script comes from the Microkit SDK; it places the PD's
# code/data and the shared-memory region symbols (fw_shmem_*) we
# reference in firewall.c.
LINKER_SCRIPT ?= $(MICROKIT_LIB)/microkit_pd.ld

$(BUILD_DIR)/$(PD_NAME).elf: $(OBJS)
	$(CC) $(PD_LDFLAGS) -T $(LINKER_SCRIPT) \
		$(OBJS) $(PD_LDLIBS) -o $@
	$(SIZE) $@

# Host-side syntax check: compiles every TU with the standard hosted
# compiler against stubbed Microkit/sDDF headers (in tools/stubs/) so
# the build can be smoke-tested without a full SDK. Does NOT link.
STUB_INC ?= tools/stubs
syntax-check: $(OBJS:.o=.su)
	@for f in $(SRCS_C); do \
		echo "  CHECK $$f"; \
		gcc -std=c11 -Wall -Wextra -Wno-unused-parameter \
			-ffreestanding -Iinclude -I$$(STUB_INC) \
			-DFSINT=1 -c $$f -o /dev/null; \
	done

clean:
	rm -rf $(BUILD_DIR)
