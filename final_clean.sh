#!/bin/bash
cd /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS

# Remove ALL temp scripts (anything starting with _ and ending in .sh or .ps1)
rm -f _*.sh _*.ps1 _headers_dump.txt 2>/dev/null

# Verify no temp files remain
echo "=== Remaining _ temp files ==="
ls _* 2>/dev/null || echo "(none)"

# Add + commit + push
git add -A
git commit -m "[chore] remove all temp scripts from repo root" 2>&1 | tail -3
git push origin main 2>&1 | tail -5

echo ""
echo "=== FINAL STATE ==="
echo "Commits: $(git rev-list --count HEAD)"
echo "Latest: $(git log --oneline -1)"
echo ""
echo "=== Untracked files ==="
git status --short | head -5
