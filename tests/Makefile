# WLTIOS Test Runner Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -O2 -g -w -Dbool=int -Dtrue=1 -Dfalse=0 -DCHAR16=uint16_t -include stdarg.h
BOOT_INC = -Isrc/boot/wlt-boot/include
INIT_INC = -Isrc/userspace/wlt-init/include
OUT = /tmp/wlt-test

.PHONY: all test clean
all: test

test:
	@echo "=== WLTIOS Test Suite ==="
	@echo "--- SHA-256 ---"
	@$(CC) $(CFLAGS) $(BOOT_INC) -o $(OUT)-sha tests/boot/sha256_test.c src/boot/wlt-boot/verify.c src/boot/wlt-boot/console.c && $(OUT)-sha || echo FAIL
	@echo "--- Config ---"
	@$(CC) $(CFLAGS) $(BOOT_INC) -o $(OUT)-cfg tests/boot/config_test.c src/boot/wlt-boot/config.c && $(OUT)-cfg || echo FAIL
	@echo "--- Service ---"
	@$(CC) $(CFLAGS) $(INIT_INC) -o $(OUT)-svc tests/userspace/service_test.c src/userspace/wlt-init/service.c src/userspace/wlt-init/dependency.c src/userspace/wlt-init/log.c src/userspace/wlt-init/tor.c && $(OUT)-svc || echo FAIL
	@echo "--- Dependency ---"
	@$(CC) $(CFLAGS) $(INIT_INC) -o $(OUT)-dep tests/userspace/dependency_test.c src/userspace/wlt-init/dependency.c src/userspace/wlt-init/service.c src/userspace/wlt-init/log.c src/userspace/wlt-init/tor.c && $(OUT)-dep || echo FAIL
	@echo "--- Monitor ---"
	@$(CC) $(CFLAGS) $(INIT_INC) -o $(OUT)-mon tests/userspace/monitor_test.c src/userspace/wlt-init/monitor.c src/userspace/wlt-init/service.c src/userspace/wlt-init/dependency.c src/userspace/wlt-init/log.c src/userspace/wlt-init/tor.c && $(OUT)-mon || echo FAIL
	@echo "--- Framebuffer ---"
	@$(CC) $(CFLAGS) -o $(OUT)-fb tests/userspace/framebuffer_test.c && $(OUT)-fb || echo FAIL
	@echo "--- Kill Switch ---"
	@$(CC) $(CFLAGS) -o $(OUT)-ks tests/userspace/kill_switch_test.c && $(OUT)-ks || echo FAIL
	@echo "--- Amnesic ---"
	@$(CC) $(CFLAGS) -o $(OUT)-amn tests/userspace/amnesic_test.c && $(OUT)-amn || echo FAIL
	@echo "--- MAC ---"
	@$(CC) $(CFLAGS) -o $(OUT)-mac tests/userspace/mac_randomize_test.c && $(OUT)-mac || echo FAIL
	@echo "--- Sandbox ---"
	@$(CC) $(CFLAGS) -o $(OUT)-sb tests/userspace/sandbox_test.c && $(OUT)-sb || echo FAIL
	@echo "--- Kairos AI ---"
	@$(CC) $(CFLAGS) -o $(OUT)-kai tests/userspace/kairos_test.c && $(OUT)-kai || echo FAIL
	@echo "--- Launcher ---"
	@$(CC) $(CFLAGS) -o $(OUT)-lau tests/userspace/launcher_test.c && $(OUT)-lau || echo FAIL
	@echo "--- Shell Draw ---"
	@$(CC) $(CFLAGS) -o $(OUT)-drw tests/userspace/shell_draw_test.c && $(OUT)-drw || echo FAIL
	@echo "--- Compositor ---"
	@$(CC) $(CFLAGS) -o $(OUT)-cmp tests/userspace/compositor_status_test.c && $(OUT)-cmp || echo FAIL
	@echo "--- Terminal ---"
	@$(CC) $(CFLAGS) -o $(OUT)-term tests/userspace/terminal_test.c && $(OUT)-term || echo FAIL
	@echo "--- Package Manager ---"
	@$(CC) $(CFLAGS) -o $(OUT)-pkg tests/userspace/pkg_test.c && $(OUT)-pkg || echo FAIL
	@echo "--- Kairos Tools ---"
	@$(CC) $(CFLAGS) -o $(OUT)-kt tests/userspace/kairos_tools_test.c && $(OUT)-kt || echo FAIL
	@echo "--- LUKS ---"
	@$(CC) $(CFLAGS) -o $(OUT)-luks tests/userspace/luks_test.c && $(OUT)-luks || echo FAIL
	@echo "=== All Tests Complete ==="

clean:
	rm -f $(OUT)-*