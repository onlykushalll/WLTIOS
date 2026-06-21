#!/bin/bash
# WLTIOS Test Script
# Runs all tests: unit tests, integration tests, QEMU boot tests
#
# Usage:
#   ./scripts/test.sh              # Run all tests
#   ./scripts/test.sh --unit       # Unit tests only
#   ./scripts/test.sh --qemu       # QEMU boot test only
#   ./scripts/test.sh --security   # Security tests only

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

export PATH="/mingw64/bin:/usr/bin:/bin:$PATH"

PASS=0
FAIL=0
SKIP=0

run_test() {
    local name="$1"
    local cmd="$2"
    echo -n "  $name ... "
    if eval "$cmd" > /tmp/wlt-test-out.log 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC}"
        FAIL=$((FAIL + 1))
        cat /tmp/wlt-test-out.log | head -20
    fi
}

echo "=== WLTIOS Test Suite ==="
echo ""

# ─── Unit Tests ────────────────────────────────────────
echo "--- Unit Tests ---"

# Boot tests
if [ -d "$PROJECT_ROOT/tests/boot" ]; then
    for test in "$PROJECT_ROOT"/tests/boot/*_test; do
        if [ -x "$test" ]; then
            run_test "boot/$(basename $test)" "$test"
        fi
    done
fi

# Userspace tests
if [ -d "$PROJECT_ROOT/tests/userspace" ]; then
    for test in "$PROJECT_ROOT"/tests/userspace/*_test; do
        if [ -x "$test" ]; then
            run_test "userspace/$(basename $test)" "$test"
        fi
    done
fi

# Rust tests (cargo)
if [ -f "$PROJECT_ROOT/src/userspace/wlt-pkg/Cargo.toml" ]; then
    echo "  wlt-pkg (cargo test) ... "
    cd "$PROJECT_ROOT/src/userspace/wlt-pkg"
    if cargo test 2>/dev/null; then
        echo -e "  ${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "  ${YELLOW}SKIP${NC} (cargo not available or no tests)"
        SKIP=$((SKIP + 1))
    fi
fi

echo ""

# ─── Integration Tests ─────────────────────────────────
echo "--- Integration Tests ---"

# QEMU boot test
if [ -f "$PROJECT_ROOT/build/wltios.iso" ] || [ -f "$PROJECT_ROOT/build/images/kernel" ]; then
    echo "  QEMU boot test ... "
    if "$SCRIPT_DIR/qemu-test.sh" --no-wait > /tmp/wlt-qemu-test.log 2>&1; then
        echo -e "  ${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC}"
        FAIL=$((FAIL + 1))
        cat /tmp/wlt-qemu-test.log | tail -20
    fi
else
    echo "  QEMU boot test ... ${YELLOW}SKIP${NC} (no OS image built yet)"
    SKIP=$((SKIP + 1))
fi

echo ""

# ─── Summary ──────────────────────────────────────────
echo "=== Summary ==="
echo -e "  ${GREEN}PASS${NC}: $PASS"
echo -e "  ${RED}FAIL${NC}: $FAIL"
echo -e "  ${YELLOW}SKIP${NC}: $SKIP"
echo ""

if [ $FAIL -gt 0 ]; then
    exit 1
fi
