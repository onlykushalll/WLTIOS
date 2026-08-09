DefinitionBlock ("wltios_dsdt.aml", "DSDT", 2, "WLTIOS", "WLTIOS1", 1)
{
    Scope (\_SB) {
        Device (PCI0) {
            Name (_HID, EisaId ("PNP0A03"))
            Name (_BBN, Zero)
            Name (_SEG, Zero)
            Name (RES0, ResourceTemplate () {
                WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                        0x0000, 0x0CF8, 0x0CFF, 0x0000, 0x0008, ,, )
                WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                        0x0000, 0xC000, 0xCFFF, 0x0000, 0x1000, ,, )
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed,
                             NonCacheable, ReadWrite,
                             0x00000000, 0xA0000000, 0xDFFFFFFF, 0x00000000,
                             0x40000000, ,, )
            })
            Method (_CRS, 0, NotSerialized) { Return (RES0) }
        }
    }
}
