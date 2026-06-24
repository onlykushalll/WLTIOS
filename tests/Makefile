# WLTIOS Test Runner Makefile
# Compiles and runs all unit tests
#
# Usage (from project root):
#   make -f tests/Makefile test       # Run all tests
#   make -f tests/Makefile clean      # Clean test binaries

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -O2 -g -w \
	 -Dbool=int -Dtrue=1 -Dfalse=0 -DCHAR16=uint16_t \
	 -include stdarg.h

BOOT_INC = -Isrc/boot/wlt-boot/include
INIT_INC = -Isrc/userspace/wlt-init/include

# Test binaries (output to /tmp to avoid polluting repo)
OUT = /tmp/wlt-test

.PHONY: all test clean

all: test

test:
	@echo "=== WLTIOS Test Suite ==="
	@echo ""
	@echo "--- SHA-256 (NIST FIPS 180-4) ---"
	@$(CC) $(CFLAGS) $(BOOT_INC) -o $(OUT)-sha tests/boot/sha256_test.c src/boot/wlt-boot/verify.c src/boot/wlt-boot/console.c && $(OUT)-sha || echo "FAIL"
	@echo ""
	@echo "--- Config Parser ---"
	@$(CC) $(CFLAGS) $(BOOT_INC) -o $(OUT)-cfg tests/boot/config_test.c src/boot/wlt-boot/config.c && $(OUT)-cfg || echo "FAIL"
	@echo ""
	@echo "--- Service Parser ---"
	@$(CC) $(CFLAGS) $(INIT_INC) -o $(OUT)-svc tests/userspace/service_test.c src/userspace/wlt-init/service.c src/userspace/wlt-init/dependency.c src/userspace/wlt-init/log.c src/userspace/wlt-init/tor.c && $(OUT)-svc || echo "FAIL"
	@echo ""
	@echo "--- Dependency Resolver ---"
	@$(CC) $(CFLAGS) $(INIT_INC) -o $(OUT)-dep tests/userspace/dependency_test.c src/userspace/wlt-init/dependency.c src/userspace/wlt-init/service.c src/userspace/wlt-init/log.c src/userspace/wlt-init/tor.c && $(OUT)-dep || echo "FAIL"
	@echo ""
	@echo "--- Process Monitor ---"
	@$(CC) $(CFLAGS) $(INIT_INC) -o $(OUT)-mon tests/userspace/monitor_test.c src/userspace/wlt-init/monitor.c src/userspace/wlt-init/service.c src/userspace/wlt-init/dependency.c src/userspace/wlt-init/log.c src/userspace/wlt-init/tor.c && $(OUT)-mon || echo "FAIL"
	@echo ""
	@echo "--- Framebuffer + Bitmap Font ---"
	@$(CC) $(CFLAGS) -o $(OUT)-fb tests/userspace/framebuffer_test.c && $(OUT)-fb || echo "FAIL"
	@echo ""
	@echo "--- Kill Switch ---"
	@$(CC) $(CFLAGS) -o $(OUT)-ks tests/userspace/kill_switch_test.c && $(OUT)-ks || echo "FAIL"
	@echo ""
	@echo "--- Amnesic Mode ---"
	@$(CC) $(CFLAGS) -o $(OUT)-amn tests/userspace/amnesic_test.c && $(OUT)-amn || echo "FAIL"
	@echo ""
	@echo "--- MAC Randomization ---"
	@$(CC) $(CFLAGS) -o $(OUT)-mac tests/userspace/mac_randomize_test.c && $(OUT)-mac || echo "FAIL"
	@echo ""
	@echo "--- Sandbox ---"
	@$(CC) $(CFLAGS) -o $(OUT)-sb tests/userspace/sandbox_test.c && $(OUT)-sb || echo "FAIL"
	@echo ""
	@echo "=== All Tests Complete ==="

clean:
	rm -f $(OUT)-sha $(OUT)-cfg $(OUT)-svc $(OUT)-dep $(OUT)-mon $(OUT)-fb $(OUT)-ks $(OUT)-amn $(OUT)-mac $(OUT)-sb