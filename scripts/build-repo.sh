#!/bin/bash
set -e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
PKG_DIR=$REPO/packages
REPO_DIR=$PKG_DIR/repo
mkdir -p $REPO_DIR
echo "=== Build 3 packages ==="
gcc -static -O2 -o /tmp/hello-wlt $PKG_DIR/hello-wlt.c 2>&1
cd /tmp && tar czf $REPO_DIR/hello-wlt-1.0.0-wlt.tar.gz hello-wlt
echo "hello-wlt built"
gcc -static -O2 -o /tmp/wlt-calc $PKG_DIR/wlt-calc.c -lm 2>&1
cd /tmp && tar czf $REPO_DIR/wlt-calc-1.0.0-wlt.tar.gz wlt-calc
echo "wlt-calc built"
gcc -static -O2 -o /tmp/wlt-status $PKG_DIR/wlt-status.c 2>&1
cd /tmp && tar czf $REPO_DIR/wlt-status-1.0.0-wlt.tar.gz wlt-status
echo "wlt-status built"
echo "=== Generate repo index ==="
cd $REPO_DIR
python3 -c "
import os, hashlib, json
packages = []
for f in sorted(os.listdir('.')):
    if f.endswith('.tar.gz'):
        h = hashlib.sha256(open(f,'rb').read()).hexdigest()
        size = os.path.getsize(f)
        name = f.replace('-1.0.0-wlt.tar.gz','')
        packages.append({'name': name, 'version': '1.0.0', 'file': f, 'sha256': h, 'size': size})
index = {'repo': 'WLTIOS Package Repository', 'version': '1', 'packages': packages}
json.dump(index, open('repo-index.json','w'), indent=2)
print(f'Repo index: {len(packages)} packages')
for p in packages:
    print(f'  {p[\"name\"]} {p[\"version\"]} ({p[\"size\"]} bytes)')
"
echo "=== Repo contents ==="
ls -la $REPO_DIR/
