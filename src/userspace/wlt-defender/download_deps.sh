#!/usr/bin/env bash
# Vendor the defender's system-library dependencies into local-deps/
# (script-relative -- works from any checkout location; the old version
# hardcoded Desktop/guardian, a path from a previous project layout).
# NOTE: `apt-get download` does NOT resolve dependencies -- every runtime
# package must be listed explicitly. libyara10 + yara below are what the
# old script missed: libyara-dev alone ships no runtime libyara.so.10.0.0
# and no CLI, which left `make sigs`/guardian against a broken libyara.so
# symlink (Error 127 / silent static-lib fallback).
set -e

SCRIPT_DIR="$(cd "$(dirname "$BASH_SOURCE[0]")" && pwd)"
DEST="${WLTIOS_DEPS_DEST:-$SCRIPT_DIR/local-deps}"

mkdir -p /tmp/apt-download
cd /tmp/apt-download
rm -f *.deb
apt-get download \
    libbpf-dev libyara-dev libyara10 yara \
    libelf-dev zlib1g-dev libsqlite3-dev libssl-dev

mkdir -p "$DEST"
for deb in *.deb; do
    dpkg-deb -x "$deb" "$DEST"
done

# --- yara CLI: real binaries + LD_LIBRARY_PATH wrappers ---------------
# The wrapper resolves libyara.so.10 relative to its own location, so the
# vendored yarac works without the conda env activated (the Makefile sigs
# target calls local-deps/usr/bin/yarac directly). Idempotent: re-running
# over an already-wrapped tree only rewrites the (identical) wrappers.
if [ -f "$DEST/usr/bin/yarac" ] && ! grep -q "yarac\.real" "$DEST/usr/bin/yarac" 2>/dev/null; then
    mv "$DEST/usr/bin/yarac" "$DEST/usr/bin/yarac.real"
    mv "$DEST/usr/bin/yara"  "$DEST/usr/bin/yara.real"
fi
for tool in yara yarac; do
    sed "s/@TOOL@/$tool/g" <<'TPL' > "$DEST/usr/bin/$tool"
#!/bin/sh
# Wrapper: Ubuntu yara 4.5.5 CLI needs libyara.so.10 next to the module's
# vendored libs. Resolve the lib dir relative to this script's location.
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$DIR/../lib/x86_64-linux-gnu:$DIR/../lib:$DIR/../lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$DIR/@TOOL@.real" "$@"
TPL
    chmod +x "$DEST/usr/bin/$tool"
done

echo "Extraction completed successfully -> $DEST"
