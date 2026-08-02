#!/bin/bash
LOG=/home/kushal/torpd-c1-process-listing.log

echo "=== Boot modules ==="
strings "$LOG" | grep -i "module\|boot.*config\|Detected.*boot" | head -10

echo ""
echo "=== Search for wlt-init-stub in boot log ==="
strings "$LOG" | grep "wlt-init-stub" | head -5

echo ""
echo "=== Search for 'Run /init' or 'Run /sbin/init' ==="
strings "$LOG" | grep -i "Run /" | head -5

echo ""
echo "=== Search for 'Starting' around init time ==="
strings "$LOG" | grep "^\[    [23]\." | grep -v "io scheduler\|drm\|ACPI\|random\|e100\|USB\|nf_\|ip_\|hid\|i2c\|micro\|console\|netcon\|Register\|taskstats\|Block\|NFS\|squash\|futex\|working\|Mount\|Disab\|Last level\|CPU:\|Perform\|WARNING\|Modules\|Call Trace\|kernel/time" | head -30

echo ""
echo "=== Check for 'request_module' ==="
strings "$LOG" | grep "request_module\|modprobe" | head -5

echo ""
echo "=== Search the capdl-loader for wlt-init-stub as a boot module ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/images/capdl-loader-image-x86_64-pc99 | grep "wlt-init" | head -10

echo ""
echo "=== Check the capdl spec (cdl file) for init modules ==="
grep -i "init\|wlt" /home/kushal/wltios-build/build-tor-pd-vm1/minimal_tor_pd.cdl 2>/dev/null | head -20

echo ""
echo "=== Check the fserv (file server) for wlt-init-stub ==="
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/archive.fserv_cpio_archive.o.cpio 2>/dev/null | grep -i "wlt\|init" | head -10
