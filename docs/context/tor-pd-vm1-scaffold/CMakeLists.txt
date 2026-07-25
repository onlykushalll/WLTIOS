cmake_minimum_required(VERSION 3.8.2)
project(tor_pd_vm1)

include(${CAMKES_VM_HELPERS_PATH})
find_package(camkes-vm-linux REQUIRED)
camkes_vm_linux_import_project()
include(${CAMKES_VM_LINUX_HELPERS_PATH})

# vm0 gets cross_vm_connections.c, vm1 gets cross_vm_connections_vm1.c
DeclareCAmkESVM(Init0 EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/cross_vm_connections.c)
DeclareCAmkESVM(Init1 EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/cross_vm_connections_vm1.c)

GetDefaultLinuxKernelFile(kernel_file)
GetDefaultLinuxRootfsFile(rootfs_file)

DecompressLinuxKernel(extract_linux_kernel decompressed_kernel ${kernel_file})
AddToFileServer("bzimage" ${decompressed_kernel} DEPENDS extract_linux_kernel)
AddToFileServer("rootfs.cpio" ${rootfs_file})

DeclareCAmkESVMRootServer(minimal_tor_pd.camkes)
