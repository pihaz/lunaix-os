#ifndef __LUNAIX_MCFG_H
#define __LUNAIX_MCFG_H

#include "sdt.h"

struct acpi_mcfg_alloc
{
    uint32_t base_addr_lo;
    uint32_t base_addr_hi;
    uint16_t pci_seg_num;
    uint8_t pci_bus_start;
    uint8_t pci_bus_end;
    uint32_t reserve;
} ACPI_TABLE_PACKED;

struct mcfg_alloc_info
{
    uint32_t base_addr;
    uint16_t pci_seg_num;
    uint8_t pci_bus_start;
    uint8_t pci_bus_end;
};

struct acpi_mcfg_toc
{
    size_t alloc_num;
    struct mcfg_alloc_info* allocations;
};

#endif /* __LUNAIX_MCFG_H */
