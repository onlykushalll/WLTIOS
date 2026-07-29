# VMM Triple Fault + vm1 Boot Fix

## Problem
vm1 crashed with EXIT_REASON_TRIPLE_FAULT (0x2) because main.c patches
skipped low-memory BIOS registration for vm1.

## Fix
Reverted the main.c patches. The optiplex9020 pattern does NOT patch
main.c — both VMs share the same guest_ram_regions/guest_fake_devices/
free_anonymous_regions.

## Result (raw serial log, 36970 bytes)
  Line 516: Welcome to Buildroot (vm0)
  Line 517: buildroot login: (vm0)
  Line 518: Welcome to Buildroot (vm1)
  Line 519: buildroot login: (vm1)
  VM_FATAL=0, triple=0. Both VMs boot to Buildroot login.
