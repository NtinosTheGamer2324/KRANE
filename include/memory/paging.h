#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_SIZE     4096u
#define PAGE_ENTRIES  1024u

#define PAGE_PRESENT  0x1u
#define PAGE_RW       0x2u
#define PAGE_USER     0x4u

/* Number of page tables identity-mapped at boot: 4 tables * 4MB each = 16MB.
 * Covers the bootloader, PMM bitmap, and a chunk of heap space. Raise this
 * if KRANE's footprint grows. */
#define PAGING_BOOT_IDENTITY_TABLES 4

/* Builds an identity-mapped page directory/tables for the first
 * PAGING_BOOT_IDENTITY_TABLES * 4MB of physical memory and enables
 * 32-bit paging (sets CR3 and the PG bit in CR0). */
void paging_init(void);

/* Maps a single 4KB page. Allocates a new page table via the PMM on
 * demand if the containing 4MB region isn't mapped yet. Returns 0 on
 * success, -1 if the PMM is out of memory. */
int paging_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);

/* Clears a single page table entry and invalidates the TLB for it. */
void paging_unmap_page(uint32_t virt_addr);

#endif // PAGING_H