#!/bin/bash
# C.1-find: Locate wlt-init-stub.c source in the build tree.
# The boot messages ("=== WLTIOS Boot ===", "[wlt-init]") are in the
# compiled capdl-loader binary. Find the source.
echo "=== 1. Search for wlt-init-stub.c by name ==="
find /home/kushal/wltios-build -name "wlt-init-stub*" -o -name "wlt_init_stub*" 2>/dev/null | head -10

echo ""
echo "=== 2. grep for the exact strings in all .c/.h files ==="
grep -rln "WLTIOS Boot" /home/kushal/wltios-build/projects/ 2>/dev/null | head -10
echo "---"
grep -rln "wlt-init\|WLT-Shell\|WLT-Init-Stub" /home/kushal/wltios-build/projects/ 2>/dev/null | head -10

echo ""
echo "=== 3. Check capdl-loader build deps ==="
# The capdl-loader is built from capdl-loader-app/src/main.c + linked libs.
# Check what objects are linked.
grep -A30 "capdl-loader" /home/kushal/wltios-build/build-tor-pd-vm1/build.ninja 2>/dev/null | grep -E "\.o|\.c" | head -20

echo ""
echo "=== 4. Check for wlt-init in the build dir objects ==="
find /home/kushal/wltios-build/build-tor-pd-vm1 -name "*.o" 2>/dev/null | xargs grep -l "WLTIOS Boot" 2>/dev/null | head -5

echo ""
echo "=== 5. Check libsel4platsupport (bootinfo.c was in the strings match) ==="
grep -n "WLTIOS\|wlt-init\|WLT-Shell" /home/kushal/wltios-build/projects/seL4_libs/libsel4platsupport/src/bootinfo.c 2>/dev/null | head -10

echo ""
echo "=== 6. Broad search: any file containing 'wlt-init-stub' ==="
find /home/kushal/wltios-build -name "*.c" -o -name "*.h" -o -name "*.cmake" -o -name "CMakeLists.txt" 2>/dev/null | xargs grep -l "wlt-init-stub\|wlt_init_stub" 2>/dev/null | head -10

echo ""
echo "=== 7. Check the capdl-loader link command in build.ninja ==="
grep -B2 -A5 "Linking C executable capdl-loader" /home/kushal/wltios-build/build-tor-pd-vm1/build.ninja 2>/dev/null | head -20
