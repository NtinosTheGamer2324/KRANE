#ifndef E820_H
#define E820_H

#include <stdint.h>

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t acpi;
} __attribute__((packed)) e820_entry_t;

#define E820_TYPE_USABLE       1
#define E820_TYPE_RESERVED     2
#define E820_TYPE_ACPI_RECLAIM 3
#define E820_TYPE_ACPI_NVS     4
#define E820_TYPE_BAD          5

/* Filled in by stage2 (detect_memory) before jumping to the C kernel. */
#define E820_COUNT_ADDR   0x500
#define E820_ENTRIES_ADDR 0x508

#endif // E820_H