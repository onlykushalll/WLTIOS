cmake_minimum_required(VERSION 3.8.2)
project(tor_pd)

include(${CAMKES_VM_HELPERS_PATH})
find_package(camkes-vm-linux REQUIRED)
camkes_vm_linux_import_project()
include(${CAMKES_VM_LINUX_HELPERS_PATH})


DeclareCAmkESVM(Init0 EXTRA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/cross_vm_connections.c)

GetDefaultLinuxKernelFile(kernel_file)
GetDefaultLinuxRootfsFile(rootfs_file)

DecompressLinuxKernel(extract_linux_kernel decompressed_kernel ${kernel_file})
AddToFileServer("bzimage" ${decompressed_kernel} DEPENDS extract_linux_kernel)
AddToFileServer("rootfs.cpio" ${rootfs_file})

DeclareCAmkESVMRootServer(
    minimal_tor_pd.camkes
    CPP_INCLUDES
    ${CAMKES_VM_DIR}/components/KeyVaultPD/include
    ${CAMKES_VM_DIR}/components/TorPD/include
)
