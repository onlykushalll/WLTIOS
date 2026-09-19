/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20251212 (64-bit version)
 * Copyright (c) 2000 - 2025 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of wltios_v2.aml
 *
 * Original Table Header:
 *     Signature        "DSDT"
 *     Length           0x000000AE (174)
 *     Revision         0x02
 *     Checksum         0x46
 *     OEM ID           "WLTIOS"
 *     OEM Table ID     "WLTIOS1"
 *     OEM Revision     0x00000001 (1)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20251212 (539300370)
 */
DefinitionBlock ("", "DSDT", 2, "WLTIOS", "WLTIOS1", 0x00000001)
{
    Scope (_SB)
    {
        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A03") /* PCI Bus */)  // _HID: Hardware ID
            Name (_BBN, Zero)  // _BBN: BIOS Bus Number
            Name (_SEG, Zero)  // _SEG: PCI Segment
            Name (RES0, ResourceTemplate ()
            {
                WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x0000,             // Granularity
                    0x0CF8,             // Range Minimum
                    0x0CFF,             // Range Maximum
                    0x0000,             // Translation Offset
                    0x0008,             // Length
                    ,, , TypeStatic, DenseTranslation)
                WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x0000,             // Granularity
                    0xC000,             // Range Minimum
                    0xCFFF,             // Range Maximum
                    0x0000,             // Translation Offset
                    0x1000,             // Length
                    ,, , TypeStatic, DenseTranslation)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0xA0000000,         // Range Minimum
                    0xDFFFFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x40000000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                Return (RES0) /* \_SB_.PCI0.RES0 */
            }

            Name (_PRT, Package (0x01)  // _PRT: PCI Routing Table
            {
                Package (0x04)
                {
                    0x0001FFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }
            })
        }
    }
}

