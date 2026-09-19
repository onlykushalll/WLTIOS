#!/usr/bin/env bash
set -e
mkdir -p /tmp/clang-download
cd /tmp/clang-download
rm -f *.deb
apt-get download clang-18 libclang-cpp18 libllvm18 libclang-common-18-dev llvm-18-linker-tools libclang1-18
# Script-relative destination (the old version hardcoded Desktop/guardian,
# a stale path from a previous project layout).
SCRIPT_DIR="$(cd "$(dirname "$BASH_SOURCE[0]")" && pwd)"
DEST="$SCRIPT_DIR/local-deps"
mkdir -p "$DEST"
for deb in *.deb; do
    dpkg-deb -x "$deb" "$DEST"
done
echo "Clang extraction completed successfully."
