#!/bin/bash
cd /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS

# Remove the temp script that got committed
rm -f _commit_push.sh
git add -A
git commit -m "[chore] remove accidentally-committed temp script"
git push origin main 2>&1 | tail -5

echo ""
echo "=== Final git log ==="
git log --oneline -5
echo ""
echo "=== Total commits ==="
git rev-list --count HEAD
echo ""
echo "=== Remote status ==="
git status -sb | head -3
