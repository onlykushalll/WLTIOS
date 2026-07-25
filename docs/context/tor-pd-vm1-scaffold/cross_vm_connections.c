#include <camkes.h>
#include <camkes/dataport_caps.h>
#include <sel4vm/guest_vm.h>
#include <sel4vmmplatsupport/drivers/cross_vm_connection.h>
#include <sel4vmmplatsupport/drivers/pci_helper.h>
#include <crossvm.h>

extern dataport_caps_handle_t tor_src_dp_handle;
extern dataport_caps_handle_t tor_dest_dp_handle;

static struct camkes_crossvm_connection connections[] = {
    {&tor_src_dp_handle, tor_ready_emit, {.id = -1, .reg_callback = NULL}},
    {&tor_dest_dp_handle, NULL, {.id = 1, .reg_callback = tor_done_reg_callback}}
};

int camkes_cross_vm_connections_init(vm_t *vm, vmm_pci_space_t *pci, seL4_CPtr irq_notification, uintptr_t connection_base_address) {
    return cross_vm_connections_init(vm, connection_base_address, connections, ARRAY_SIZE(connections), pci, irq_notification);
}
