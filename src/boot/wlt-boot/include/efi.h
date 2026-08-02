/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Vendored UEFI type definitions — extracted from gnu-efi (BSD-2-Clause).
 * Source: gnu-efi 4.0.0 (apt package), /usr/include/efi/
 *
 * This file provides the REAL UEFI struct layouts so the bootloader
 * uses typed struct access instead of magic byte offsets.
 * Generated for WLTIOS Section A.1 (offset verification fix).
 */
#ifndef WLT_EFI_H
#define WLT_EFI_H

#include <stdint.h>
#include <stddef.h>

/* ─── Basic UEFI types ─── */
typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint64_t UINTN;       /* 8 bytes on 64-bit */
typedef int64_t  INTN;
typedef uint16_t CHAR16;
typedef uint64_t EFI_STATUS;
typedef void   *EFI_HANDLE;
typedef void   *EFI_PHYSICAL_ADDRESS;
typedef uint32_t EFI_ALLOCATE_TYPE;

#define EFI_SUCCESS             0
#define EFI_BUFFER_TOO_SMALL    (0x8000000000000005ULL)
#define EFI_INVALID_PARAMETER   (0x8000000000000002ULL)

/* ─── EFI_TABLE_HEADER (24 bytes) ─── */
typedef struct {
    UINT64  Signature;
    UINT32  Revision;
    UINT32  HeaderSize;
    UINT32  CRC32;
    UINT32  Reserved;
} EFI_TABLE_HEADER;

/* ─── EFI_MEMORY_DESCRIPTOR ─── */
typedef struct {
    UINT32                  Type;
    UINT32                  PhysicalStart;
    UINT64                  VirtualStart;
    UINT64                  NumberOfPages;
    UINT64                  Attribute;
} EFI_MEMORY_DESCRIPTOR;

/* ─── EFI_BOOT_SERVICES (real layout from gnu-efi efiapi.h) ─── */
typedef struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER              Hdr;                        /* 0   */
    /* Task priority */
    void                         *RaiseTPL;                   /* 24  */
    void                         *RestoreTPL;                 /* 32  */
    /* Memory services */
    EFI_STATUS                  (*AllocatePages)(EFI_ALLOCATE_TYPE, UINT32, UINTN, EFI_PHYSICAL_ADDRESS *);  /* 40 */
    EFI_STATUS                  (*FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);                                      /* 48 */
    EFI_STATUS                  (*GetMemoryMap)(UINTN *, EFI_MEMORY_DESCRIPTOR *, UINTN *, UINTN *, UINT32 *); /* 56 */
    EFI_STATUS                  (*AllocatePool)(UINT32, UINTN, void **);                                        /* 64 */
    EFI_STATUS                  (*FreePool)(void *);                                                           /* 72 */
    /* Event & timer */
    void                         *CreateEvent;                /* 80  */
    void                         *SetTimer;                   /* 88  */
    void                         *WaitForEvent;               /* 96  */
    void                         *SignalEvent;                /* 104 */
    void                         *CloseEvent;                 /* 112 */
    void                         *CheckEvent;                 /* 120 */
    /* Protocol handler */
    void                         *InstallProtocolInterface;   /* 128 */
    void                         *ReinstallProtocolInterface; /* 136 */
    void                         *UninstallProtocolInterface; /* 144 */
    void                         *HandleProtocol;             /* 152 */
    void                         *PCHandleProtocol;           /* 160 */
    void                         *RegisterProtocolNotify;     /* 168 */
    void                         *LocateHandle;               /* 176 */
    void                         *LocateDevicePath;           /* 184 */
    void                         *InstallConfigurationTable;  /* 192 */
    /* Image services */
    void                         *LoadImage;                  /* 200 */
    void                         *StartImage;                 /* 208 */
    void                         *Exit;                       /* 216 */
    void                         *UnloadImage;                /* 224 */
    EFI_STATUS                  (*ExitBootServices)(EFI_HANDLE, UINTN);  /* 232 */
    /* Misc */
    void                         *GetNextMonotonicCount;      /* 240 */
    void                         *Stall;                      /* 248 */
    void                         *SetWatchdogTimer;           /* 256 */
    /* Driver support */
    void                         *ConnectController;          /* 264 */
    void                         *DisconnectController;       /* 272 */
    /* Open/Close protocol */
    void                         *OpenProtocol;               /* 280 */
    void                         *CloseProtocol;              /* 288 */
    void                         *OpenProtocolInformation;    /* 296 */
    /* Library */
    void                         *ProtocolsPerHandle;         /* 304 */
    void                         *LocateHandleBuffer;         /* 312 */
    EFI_STATUS                  (*LocateProtocol)(void *Protocol, void *Registration, void **Interface);  /* 320 */
    void                         *InstallMultipleProtocolInterfaces;  /* 328 */
    void                         *UninstallMultipleProtocolInterfaces; /* 336 */
    /* CRC */
    void                         *CalculateCrc32;             /* 344 */
    /* Misc */
    void                         *CopyMem;                    /* 352 */
    void                         *SetMem;                     /* 360 */
    void                         *CreateEventEx;              /* 368 */
} EFI_BOOT_SERVICES;

/* ─── EFI_GUID ─── */
typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

/* ─── EFI_SYSTEM_TABLE (real layout) ─── */
typedef struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER              Hdr;                    /* 0   */
    CHAR16                       *FirmwareVendor;         /* 24  */
    UINT32                        FirmwareRevision;       /* 32  */
    UINT32                        _pad1;                  /* 36  */
    EFI_HANDLE                    ConsoleInHandle;        /* 40  */
    void                         *ConIn;                  /* 48  */
    EFI_HANDLE                    ConsoleOutHandle;       /* 56  */
    void                         *ConOut;                 /* 64  */
    EFI_HANDLE                    StandardErrorHandle;    /* 72  */
    void                         *StdErr;                 /* 80  */
    void                         *RuntimeServices;        /* 88  */
    EFI_BOOT_SERVICES            *BootServices;           /* 96  */
    UINTN                         NumberOfTableEntries;   /* 104 */
    void                         *ConfigurationTable;     /* 112 */
} EFI_SYSTEM_TABLE;

/* ─── Memory types ─── */
#define EFI_LOADER_CODE          1
#define EFI_LOADER_DATA          2
#define EFI_BOOT_SERVICES_CODE   3
#define EFI_BOOT_SERVICES_DATA   4
#define EFI_RUNTIME_SERVICES_CODE 5
#define EFI_RUNTIME_SERVICES_DATA 6
#define EFI_CONVENTIONAL_MEMORY  7

/* ─── Allocate types ─── */
#define AllocateAnyPages         0
#define AllocateMaxAddress       1
#define AllocateAddress          2

/* ─── Protocol GUIDs ─── */
/* gEfiSimpleFileSystemProtocolGuid — the real value from gnu-efi */
/* gEfiFileInfoGuid (for EFI_FILE_PROTOCOL->GetInfo) */
static const EFI_GUID gEfiFileInfoGuid = {
    0x09576E92, 0x6D3F, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x0964E5B2, 0x6459, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

#endif /* WLT_EFI_H */
