#include <camkes.h>
#include <camkes/dataport_caps.h>
#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_vm_util.h>
#include <sel4vm/arch/ioports.h>
#include <sel4vmmplatsupport/drivers/cross_vm_connection.h>
#include <sel4vmmplatsupport/drivers/pci_helper.h>
#include <crossvm.h>
#include <stdio.h>

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
 * SIGNATURE NOTE: These wrappers match the EXACT vmm_pci_config_t signature
 * from pci.h:31-39:
 *   uint8_t  (*ioread8)(void *cookie, vmm_pci_address_t addr, unsigned int offset);
 *   uint16_t (*ioread16)(void *cookie, vmm_pci_address_t addr, unsigned int offset);
 *   uint32_t (*ioread32)(void *cookie, vmm_pci_address_t addr, unsigned int offset);
 *   void (*iowrite8)(void *cookie, vmm_pci_address_t addr, unsigned int offset, uint8_t val);
 *   void (*iowrite16)(void *cookie, vmm_pci_address_t addr, unsigned int offset, uint16_t val);
 *   void (*iowrite32)(void *cookie, vmm_pci_address_t addr, unsigned int offset, uint32_t val);
 *
 * This is identical to the pattern used by main.c's camkes_pci_read8/16/32
 * and camkes_pci_write8/16/32. The vmm_pci_entry_t.ioread/iowrite (which use
 * int return + uint32_t* result) are a DIFFERENT interface, internally
 * bridged by passthrough_pci_config_ioread/iowrite in pci_helper.c:107-143
 * which call these ioread8/16/32 callbacks and assign the return value.
 *
 * The pcnet NIC is I/O-PORT-ONLY from the driver's perspective:
 *   - BAR0 (offset 0x10): I/O ports 0xc000-0xc01f (bit0=1). The pcnet32
 *     driver talks to the NIC exclusively through these I/O ports, which are
 *     granted to the guest via the camkes vm_ioports grant (0xc000-0xc020).
 *   - BAR1 (offset 0x14): 32-byte MMIO aperture at 0xfeb91000. The pcnet32
 *     driver PROBES this but does not use it for traffic.
 */
/*
 * BDF REDIRECTION (root-cause fix, 2026-09-18):
 * vmm_pci_add_entry() IGNORES the address passed to vmm_pci_create_passthrough()
 * and stores the entry at the next free VIRTUAL slot (00:01.0 in this assembly:
 * slot 0 = emulated host bridge, then this entry). The callbacks below receive
 * the address the GUEST probed (the virtual slot), NOT the pcnet's real BDF.
 * Passing that address through to the host config space reads the WRONG device
 * (QEMU's real 00:01.0 = PIIX4 ISA bridge, 8086:7000) - the guest discovered a
 * phantom ISA bridge at the pcnet's slot and pcnet32 never bound (proven by the
 * instrumented boot: cfg_in HIT at the virtual slot returned 8086:7000).
 * Fix: redirect EVERY access to the pcnet's REAL host BDF 0:3:0 regardless of
 * the virtual slot the guest addressed.
 */
#define PCNET_REAL_BUS 0
#define PCNET_REAL_DEV 3
#define PCNET_REAL_FUN 0

static uint8_t pcnet_cfg_read8(void *cookie, vmm_pci_address_t addr, unsigned int offset) {
    (void)addr;
    uint8_t val = pci_config_read8(PCNET_REAL_BUS, PCNET_REAL_DEV, PCNET_REAL_FUN, offset);
    static int dbg_r8 = 0;
    if (dbg_r8 < 30) {
        printf("[pcnet-passthru] read8 slot=%d:%d:%d off=0x%x val=0x%02x\n",
               addr.bus, addr.dev, addr.fun, offset, val);
        dbg_r8++;
    }
    return val;
}
static uint16_t pcnet_cfg_read16(void *cookie, vmm_pci_address_t addr, unsigned int offset) {
    (void)addr;
    uint16_t val = pci_config_read16(PCNET_REAL_BUS, PCNET_REAL_DEV, PCNET_REAL_FUN, offset);
    printf("[pcnet-passthru] read16 slot=%d:%d:%d off=0x%x val=0x%04x\n",
           addr.bus, addr.dev, addr.fun, offset, val);
    return val;
}
static uint32_t pcnet_cfg_read32(void *cookie, vmm_pci_address_t addr, unsigned int offset) {
    (void)addr;
    uint32_t val = pci_config_read32(PCNET_REAL_BUS, PCNET_REAL_DEV, PCNET_REAL_FUN, offset);
    printf("[pcnet-passthru] read32 slot=%d:%d:%d off=0x%x val=0x%08x\n",
           addr.bus, addr.dev, addr.fun, offset, val);
    return val;
}
static void pcnet_cfg_write8(void *cookie, vmm_pci_address_t addr, unsigned int offset, uint8_t val) {
    (void)addr;
    pci_config_write8(PCNET_REAL_BUS, PCNET_REAL_DEV, PCNET_REAL_FUN, offset, val);
}
static void pcnet_cfg_write16(void *cookie, vmm_pci_address_t addr, unsigned int offset, uint16_t val) {
    (void)addr;
    pci_config_write16(PCNET_REAL_BUS, PCNET_REAL_DEV, PCNET_REAL_FUN, offset, val);
    printf("[pcnet-passthru] write16 slot=%d:%d:%d off=0x%x val=0x%04x\n",
           addr.bus, addr.dev, addr.fun, offset, val);
}
static void pcnet_cfg_write32(void *cookie, vmm_pci_address_t addr, unsigned int offset, uint32_t val) {
    (void)addr;
    pci_config_write32(PCNET_REAL_BUS, PCNET_REAL_DEV, PCNET_REAL_FUN, offset, val);
    printf("[pcnet-passthru] write32 slot=%d:%d:%d off=0x%x val=0x%08x\n",
           addr.bus, addr.dev, addr.fun, offset, val);
}

int camkes_cross_vm_connections_init(vm_t *vm, vmm_pci_space_t *pci, seL4_CPtr irq_notification, uintptr_t connection_base_address) {
    printf("[pcnet-passthru] camkes_cross_vm_connections_init called\n");
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
        printf("[pcnet-passthru] vmm_pci_add_entry done\n");
    }

    /* RC7 FIX (2026-09-19): QEMU's PIIX3 PIRQ routing registers
     * (0:1.0 config 0x60-0x63) default to DISABLED (0x80) and nothing
     * programs them on -kernel direct boots (SeaBIOS does not on this
     * path). The pcnet (QEMU slot 3) INTA lands on PIRQC (reg 0x62):
     * pci_slot_get_pirq() = (INTA + slot-1) & 3 = 2. With 0x62 disabled,
     * the level-low INTA never reaches any ISA IRQ, so IOAPIC pin 11
     * (the vm_irqs wiring: source=11) never fires - proven by boot
     * 2026-09-19 09:03: only timer badges (0x8020000) arrive at
     * handle_async_event, zero IRQ-11 badges (0x8001000). Route PIRQC
     * to IRQ 11 and read back all four for evidence. */
    {
        uint8_t pirq[4];
        for (int i = 0; i < 4; i++) {
            pirq[i] = pci_config_read8(0, 1, 0, 0x60 + i);
        }
        printf("[pcnet-passthru] PIIX PIRQ before: %02x %02x %02x %02x\n",
               pirq[0], pirq[1], pirq[2], pirq[3]);
        if (pirq[2] != 11) {
            pci_config_write8(0, 1, 0, 0x62, 11);
        }
        printf("[pcnet-passthru] PIIX PIRQ after:  %02x %02x %02x %02x\n",
               pci_config_read8(0, 1, 0, 0x60), pci_config_read8(0, 1, 0, 0x61),
               pci_config_read8(0, 1, 0, 0x62), pci_config_read8(0, 1, 0, 0x63));
    }

    /* RC6 FIX (2026-09-19): the pcnet32 driver is I/O-port-native (BAR0,
     * I/O 0xc000-0xc01f). The IOPort caps allocated by the vm_ioports
     * camkes attribute are NOT sufficient on their own: seL4 delivers
     * guest I/O exits to the VMM unless the vcpu's I/O bitmap explicitly
     * opens the range. main.c only does this for NON-PCI ioport entries;
     * PCI entries were assumed to be covered by the BAR-emulation device
     * loop (never exercised for this wiring). Open the range natively via
     * seL4_X86_VCPU_EnableIOPort so the guest's IN/OUT at 0xc000+ execute
     * directly against the real (QEMU) pcnet - no trap, no handler. */
    {
        vm_vcpu_t *vcpu0 = (vm->num_vcpus > 0) ? vm->vcpus[0] : NULL;
        if (vcpu0) {
            int io_err = vm_enable_passthrough_ioport(vcpu0, 0xc000, 0xc01f);
            printf("[pcnet-passthru] native I/O passthrough 0xc000-0xc01f: %s\n",
                   io_err ? "FAILED" : "enabled");
        } else {
            printf("[pcnet-passthru] native I/O passthrough: NO VCPU\n");
        }
    }

    return cross_vm_connections_init(vm, connection_base_address, connections, ARRAY_SIZE(connections), pci, irq_notification);
}
