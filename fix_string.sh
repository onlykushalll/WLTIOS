#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot

# Fix the broken string literal in loader.c
python3 << 'PYEOF'
import os
fpath = os.path.join(os.environ.get('BOOTDIR', ''), 'loader.c')
with open(fpath, 'r') as f:
    content = f.read()

# The broken string spans two lines. Fix it by joining them with \n escape.
old = '''wlt_console_printf("  Loaded %llu bytes at 0x%llx (BSS=%llu)
",'''
new = '''wlt_console_printf("  Loaded %llu bytes at 0x%llx (BSS=%llu)\\n",'''

if old in content:
    content = content.replace(old, new)
    with open(fpath, 'w') as f:
        f.write(content)
    print('loader.c: string literal fixed.')
else:
    print('loader.c: broken string not found.')
PYEOF

echo ""
echo "=== BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -20
