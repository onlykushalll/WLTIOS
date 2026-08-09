#include <camkes.h>
#include <camkes/dataport_caps.h>
#include <sel4vm/guest_vm.h>
#include <sel4vmmplatsupport/drivers/cross_vm_connection.h>
#include <sel4vmmplatsupport/drivers/pci_helper.h>
#include <crossvm.h>

extern dataport_caps_handle_t tor_src_dp_handle;
extern dataport_caps_handle_t tor_dest_dp_handle;

static struct camkes_crossvm_connection connections[] = {
    {&tor_src_dp_handle, NULL, {.id = -1, .reg_callback = NULL}},
    {&tor_dest_dp_handle, NULL, {.id = 1, .reg_callback = NULL}}
};

/* PCI config-space passthrough wrappers for the pcnet NIC (host BDF 0:3:0).
 *
 * PURE PASSTHROUGH: every wrapper forwards directly to the camkes-generated
 * pci_config_read/write RPC (which hits the host's 0xCF8/0xCFC config space)
 * with NO offset checks and NO BAR interception of any kind.
 *
 * Rationale: the previous v10 boot (pure passthrough) successfully let the
 * guest's PCI scan discover the pcnet (AMD Am79C970A, PCI 1022:2000) at
 * 0000:00:01.0. The later-added BAR1 interception (returning 0 for offset
 * 0x14 reads, dropping offset 0x14 writes) broke the entire PCI config space
 * read path -- the guest's scan found NO devices. Reverting to pure
 * passthrough restores PCI enumeration.
 *
 * The pcnet NIC is I/O-PORT-ONLY from the driver's perspective:
 *   - BAR0 (offset 0x10): I/O ports 0xc000-0xc01f (bit0=1). The pcnet32
 *     driver talks to the NIC exclusively through these I/O ports, which are
 *     granted to the guest via the camkes vm_ioports grant (0xc000-0xc020).
 *     The DSDT now exposes an I/O window (0xC000-0xCFFF) on the PCI host
 *     bridge, so the guest's PCI core can claim BAR0.
 *   - BAR1 (offset 0x14): 32-byte MMIO aperture at 0xfeb91000. The pcnet32
 *     driver PROBES this but does not use it for traffic. With pure
 *     passthrough the guest may fail to claim BAR1 (MMIO window exhaustion),
 *     but the pcnet32 driver continues to bind using BAR0 alone. This is
 *     acceptable -- the previous v10 boot worked this way.
 */
static uint8_t pcnet_cfg_read8(void *cookie, vmm_pci_address_t addr, unsigned int offset) {
    return pci_config_read8(addr.bus, addr.dev, addr.fun, offset);
}
static uint16_t pcnet_cfg_read16(void *cookie, vmm_pci_address_t addr, unsigned int offset) {
    return pci_config_read16(addr.bus, addr.dev, addr.fun, offset);
}
static uint32_t pcnet_cfg_read32(void *cookie, vmm_pci_address_t addr, unsigned int offset) {
    return pci_config_read32(addr.bus, addr.dev, addr.fun, offset);
}
static void pcnet_cfg_write8(void *cookie, vmm_pci_address_t addr, unsigned int offset, uint8_t val) {
    pci_config_write8(addr.bus, addr.dev, addr.fun, offset, val);
}
static void pcnet_cfg_write16(void *cookie, vmm_pci_address_t addr, unsigned int offset, uint16_t val) {
    pci_config_write16(addr.bus, addr.dev, addr.fun, offset, val);
}
static void pcnet_cfg_write32(void *cookie, vmm_pci_address_t addr, unsigned int offset, uint32_t val) {
    pci_config_write32(addr.bus, addr.dev, addr.fun, offset, val);
}

int camkes_cross_vm_connections_init(vm_t *vm, vmm_pci_space_t *pci, seL4_CPtr irq_notification, uintptr_t connection_base_address) {
    /* Register pcnet NIC (host BDF 0:3:0) for pure PCI config space
     * passthrough. This lets the guest discover the pcnet via PCI config
     * space scan. I/O ports (0xc000-0xc01f) and IRQ 11 are handled by camkes
     * vm_ioports/vm_irqs. No BAR interception. */
    {
        vmm_pci_config_t pcnet_cfg = {
            .cookie = NULL,
            .ioread8 = pcnet_cfg_read8,
            .ioread16 = pcnet_cfg_read16,
            .ioread32 = pcnet_cfg_read32,
            .iowrite8 = pcnet_cfg_write8,
            .iowrite16 = pcnet_cfg_write16,
            .iowrite32 = pcnet_cfg_write32
        };
        vmm_pci_entry_t pcnet_entry = vmm_pci_create_passthrough(
            (vmm_pci_address_t){0, 3, 0}, pcnet_cfg);
        /* Emulate the IRQ line register (offset 0x3C) so the guest's PCI INT
         * routing maps INT A -> IRQ 11 (matching vm_irqs source=11/dest=11).
         * Also strip MSI capabilities (pci=nomsi). pcnet has no MSI cap so
         * the no_msi call is effectively a no-op but kept for parity with the
         * standard seL4 passthrough pattern. */
        pcnet_entry = vmm_pci_create_irq_emulation(pcnet_entry, 11);
        pcnet_entry = vmm_pci_no_msi_cap_emulation(pcnet_entry);
        vmm_pci_add_entry(pci, pcnet_entry, NULL);
    }

    return cross_vm_connections_init(vm, connection_base_address, connections, ARRAY_SIZE(connections), pci, irq_notification);
}
