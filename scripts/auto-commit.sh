#!/bin/bash
# WLTIOS Auto-Commit Script
# Called by orchestrator after every approved change session
#
# Usage: ./scripts/auto-commit.sh "[module] action: description" "Spec: docs/specs/x.md"
#
# This script:
# 1. Runs pre-commit checks (security scan, lint)
# 2. Stages all changes
# 3. Commits with the provided message
# 4. Pushes to GitHub

set -e

COMMIT_MSG="${1:-[wltios] update: changes}"
SPEC_REF="${2:-}"

cd /c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS 2>/dev/null || cd "$HOME/OneDrive/Desktop/WLTIOS" 2>/dev/null || {
  echo "ERROR: Cannot find WLTIOS directory"
  exit 1
}

echo "=== WLTIOS Auto-Commit ==="
echo "Message: $COMMIT_MSG"
echo ""

# 1. Pre-commit checks
echo "[1/4] Running pre-commit checks..."

# Check for hardcoded credentials
if grep -rn -i -E "(password|secret|api_key|token)\s*=\s*['\"][^'\"]{8,}" src/ 2>/dev/null | grep -v -E "(test|example|placeholder)"; then
  echo "❌ BLOCKED: Potential hardcoded credential found!"
  grep -rn -i -E "(password|secret|api_key|token)\s*=\s*['\"][^'\"]{8,}" src/ 2>/dev/null | grep -v -E "(test|example|placeholder)"
  exit 1
fi

# Check for v1/v2/v3 files
NEW_FILES=$(git status --porcelain | grep "^??" | awk '{print $2}')
for f in $NEW_FILES; do
  if echo "$f" | grep -qi -E "(_v[0-9]|_new|_fixed|_backup|_temp|temp\.|test2)"; then
    echo "❌ BLOCKED: Forbidden filename pattern: $f"
    echo "  No _v1, _v2, _new, _fixed, _backup, _temp files allowed."
    echo "  Modify the original file instead."
    exit 1
  fi
done

echo "✅ Pre-commit checks passed"

# 2. Stage changes
echo "[2/4] Staging changes..."
git add -A

# Check if there's anything to commit
if git diff --cached --quiet; then
  echo "ℹ️  Nothing to commit (no changes)"
  exit 0
fi

# 3. Commit
echo "[3/4] Committing..."
FULL_MSG="$COMMIT_MSG"
if [ -n "$SPEC_REF" ]; then
  FULL_MSG="$FULL_MSG

$SPEC_REF"
fi

git commit -m "$FULL_MSG"
echo "✅ Committed"

# 4. Push
echo "[4/4] Pushing to GitHub..."
git push origin main
echo "✅ Pushed"

echo ""
echo "=== Done ==="
git log --oneline -1
