#!/bin/bash
# seL4 64-bit TSC frequency fix (research-G R2)
# The seL4 kernel's measure_tsc_khz() retries PIT calibration 10 times then
# fails with "TSC frequency too high" under QEMU TCG. Setting
# CONFIG_PC99_TSC_FREQUENCY skips the PIT calibration entirely.
#
# The cmake -D flag doesn't override the config_string default; this script
# patches the generated gen_config.h directly + rebuilds the kernel.
BUILD=~/wltios-build/build-keyvault-64
cd "$BUILD"
for f in kernel/gen_config/kernel/gen_config.h \
         sel4_projects_libs/libsel4vmmplatsupport/gen_config/sel4vmmplatsupport/gen_config.h; do
    [ -f "$f" ] && sed -i 's/#define CONFIG_PC99_TSC_FREQUENCY  0/#define CONFIG_PC99_TSC_FREQUENCY 2000000000/' "$f"
done
find kernel -name '*.o' -delete
ninja images/kernel-x86_64-pc99
