#!/bin/bash
# WLTIOS Pre-Commit Check
# Runs security scans, lint, and validation before every commit
# This is the guardian of the Engineering Framework
#
# Usage: Called automatically by auto-commit.sh, or manually:
#   ./scripts/pre-commit-check.sh

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

ERRORS=0
WARNINGS=0

print_ok()   { echo -e "  ${GREEN}[OK]${NC} $1"; }
print_err()  { echo -e "  ${RED}[ERROR]${NC} $1"; ERRORS=$((ERRORS + 1)); }
print_warn() { echo -e "  ${YELLOW}[WARN]${NC} $1"; WARNINGS=$((WARNINGS + 1)); }

echo "=== WLTIOS Pre-Commit Check ==="
echo ""

# ─── Check 1: No hardcoded credentials ─────────────────
echo "1. Checking for hardcoded credentials..."

# Patterns that indicate hardcoded secrets
SECRET_PATTERNS=(
    "password\s*=\s*['\"][^'\"]{8,}"
    "secret\s*=\s*['\"][^'\"]{8,}"
    "api_key\s*=\s*['\"][^'\"]{8,}"
    "token\s*=\s*['\"][^'\"]{8,}"
    "private_key\s*=\s*['\"][^'\"]{8,}"
)

FOUND_SECRETS=false
for pattern in "${SECRET_PATTERNS[@]}"; do
    if grep -rn -i -E "$pattern" "$PROJECT_ROOT/src/" 2>/dev/null | grep -v -E "(test|example|placeholder|template)"; then
        print_err "Hardcoded credential found!"
        FOUND_SECRETS=true
    fi
done

if [ "$FOUND_SECRETS" = false ]; then
    print_ok "No hardcoded credentials"
fi

# ─── Check 2: No v1/v2/v3 files ────────────────────────
echo "2. Checking for forbidden filename patterns..."

FORBIDDEN_PATTERNS="_v[0-9]|_new$|_fixed$|_backup$|_temp$|^temp\.|test2"
NEW_FILES=$(cd "$PROJECT_ROOT" && git status --porcelain | grep "^??" | awk '{print $2}')

for f in $NEW_FILES; do
    if echo "$f" | grep -qiE "$FORBIDDEN_PATTERNS"; then
        print_err "Forbidden filename: $f"
        print_err "  No _v1, _v2, _new, _fixed, _backup, _temp files allowed."
        print_err "  Modify the original file instead."
    fi
done

if [ $ERRORS -eq 0 ]; then
    print_ok "No forbidden filenames"
fi

# ─── Check 3: Module headers in new source files ───────
echo "3. Checking module headers in new source files..."

for f in $NEW_FILES; do
    if echo "$f" | grep -qE "\.(c|cpp|h|rs)$"; then
        if [ -f "$PROJECT_ROOT/$f" ]; then
            if ! head -20 "$PROJECT_ROOT/$f" | grep -q "@file\|@module\|@purpose"; then
                print_warn "Missing module header: $f"
            fi
        fi
    fi
done

if [ $WARNINGS -eq 0 ]; then
    print_ok "All new source files have headers"
fi

# ─── Check 4: No TODOs without context ─────────────────
echo "4. Checking for TODOs without context..."

if grep -rn "TODO" "$PROJECT_ROOT/src/" 2>/dev/null | grep -v "TODO(" | grep -v "TODO:" | head -5; then
    print_warn "Found TODOs without module context (use TODO(module): description)"
else
    print_ok "No bare TODOs"
fi

# ─── Check 5: .gitignore present ───────────────────────
echo "5. Checking .gitignore..."

if [ -f "$PROJECT_ROOT/.gitignore" ]; then
    print_ok ".gitignore exists"
else
    print_err ".gitignore missing!"
fi

# ─── Check 6: No files > 500 lines ─────────────────────
echo "6. Checking for files over 500 lines..."

for f in $(cd "$PROJECT_ROOT" && git diff --cached --name-only --diff-filter=ACM 2>/dev/null | grep -E "\.(c|cpp|h|rs)$"); do
    if [ -f "$PROJECT_ROOT/$f" ]; then
        LINES=$(wc -l < "$PROJECT_ROOT/$f")
        if [ $LINES -gt 500 ]; then
            print_warn "$f is $LINES lines (max 500). Consider splitting."
        fi
    fi
done

print_ok "File length check complete"

# --- Check: camkes app drift (gap #26 guard) ---------------------------
echo ""
echo "Checking camkes app sync (build tree vs git)..."
if bash "$SCRIPT_DIR/check-camkes-sync.sh"; then
    print_ok "Camkes app in sync with build tree"
else
    print_err "Camkes app DRIFT -- what boots is not what git tracks; reconcile before committing"
fi

# ─── Summary ───────────────────────────────────────────
echo ""
echo "=== Summary ==="
echo -e "  ${RED}Errors${NC}:   $ERRORS"
echo -e "  ${YELLOW}Warnings${NC}: $WARNINGS"
echo ""

if [ $ERRORS -gt 0 ]; then
    echo -e "${RED}[BLOCKED]${NC} Commit rejected — fix errors first"
    exit 1
fi

if [ $WARNINGS -gt 0 ]; then
    echo -e "${YELLOW}[WARN]${NC} Commit allowed, but review warnings"
fi

echo -e "${GREEN}[PASS]${NC} Pre-commit checks passed"
exit 0
