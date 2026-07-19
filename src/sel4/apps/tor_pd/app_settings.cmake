#
# app_settings.cmake for tor_pd
# Copyright 2018, Data61, CSIRO (ABN 41 687 119 230)
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.8.2)

# Define kernel config options (same as keyvault_pd — 32-bit ia32)
set(KernelSel4Arch ia32 CACHE STRING "" FORCE)
set(KernelMaxNumNodes 1 CACHE STRING "" FORCE)
