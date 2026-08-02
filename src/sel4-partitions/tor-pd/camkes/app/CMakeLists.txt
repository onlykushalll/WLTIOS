cmake_minimum_required(VERSION 3.8.2)
project(tor_pd_vm1)

include(${CAMKES_VM_HELPERS_PATH})
find_package(camkes-vm-linux REQUIRED)
camkes_vm_linux_import_project()
include(${CAMKES_VM_LINUX_HELPERS_PATH})

DeclareCAmkESVM(Init0 EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/cross_vm_connections.c)
DeclareCAmkESVM(Init1 EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/cross_vm_connections_vm1.c)

GetDefaultLinuxKernelFile(kernel_file)
GetDefaultLinuxRootfsFile(rootfs_file)

DecompressLinuxKernel(extract_linux_kernel decompressed_kernel ${kernel_file})
AddToFileServer("bzimage" ${decompressed_kernel} DEPENDS extract_linux_kernel)

# A.2: vm0 gets its OWN rootfs (64-bit, NO Tor)
set(rootfs_vm0_file "/home/kushal/wltios-build/rootfs_vm0_notor64.cpio")
AddToFileServer("rootfs.cpio" ${rootfs_vm0_file})

AddToFileServer("bzimage_vm1" ${decompressed_kernel} DEPENDS extract_linux_kernel)

# A.2: vm1 gets its OWN rootfs (64-bit + Tor) - genuine per-VM isolation
set(rootfs_vm1_file "/home/kushal/wltios-build/rootfs_vm1_tor64.cpio")
AddToFileServer("rootfs_vm1.cpio" ${rootfs_vm1_file})

DeclareCAmkESVMRootServer(minimal_tor_pd.camkes)
