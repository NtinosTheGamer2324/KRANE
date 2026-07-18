#include "memory/paging.h"
#include "memory/pmm.h"
#include "vga.h"

#define PLOG(str) log_str(str, 0x07)
#define PLOG_HEX32(val) log_hex_uint32((uint32_t)(val), 0x0E)

/* Identity-mapped at boot: virt == phys for the low
 * PAGING_BOOT_IDENTITY_TABLES * 4MB region. Anything mapped later via
 * paging_map_page() gets its page table allocated from the PMM, which
 * is safe precisely because that bitmap-tracked memory is also
 * identity-mapped here. */
static uint32_t page_directory[PAGE_ENTRIES] __attribute__((aligned(4096)));
static uint32_t boot_page_tables[PAGING_BOOT_IDENTITY_TABLES][PAGE_ENTRIES] __attribute__((aligned(4096)));

static inline uint32_t read_cr0(void) {
    uint32_t v;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(v));
    return v;
}

static inline uint32_t read_cr3(void) {
    uint32_t v;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void load_page_directory(uint32_t *dir) {
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(dir) : "memory");
}

static inline void enable_paging(void) {
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u; /* PG bit */
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

void paging_init(void) {
    PLOG("[paging] paging_init: start\n");
    PLOG("[paging]   page_directory @ "); PLOG_HEX32((uint32_t)page_directory); PLOG("\n");
    PLOG("[paging]   boot_page_tables @ "); PLOG_HEX32((uint32_t)boot_page_tables); PLOG("\n");
    PLOG("[paging]   PAGING_BOOT_IDENTITY_TABLES="); PLOG_HEX32(PAGING_BOOT_IDENTITY_TABLES); PLOG("\n");

    PLOG("[paging] clearing page directory entries\n");
    for (unsigned int i = 0; i < PAGE_ENTRIES; i++) {
        page_directory[i] = 0; /* not present */
    }
    PLOG("[paging] page directory cleared\n");

    for (unsigned int t = 0; t < PAGING_BOOT_IDENTITY_TABLES; t++) {
        PLOG("[paging] building identity table t="); PLOG_HEX32(t); PLOG("\n");
        for (unsigned int i = 0; i < PAGE_ENTRIES; i++) {
            uint32_t phys = (t * PAGE_ENTRIES + i) * PAGE_SIZE;
            boot_page_tables[t][i] = phys | PAGE_PRESENT | PAGE_RW;
        }
        page_directory[t] = ((uint32_t)boot_page_tables[t]) | PAGE_PRESENT | PAGE_RW;
        PLOG("[paging]   pd["); PLOG_HEX32(t); PLOG("] = "); PLOG_HEX32(page_directory[t]); PLOG("\n");
        PLOG("[paging]   first entry: "); PLOG_HEX32(boot_page_tables[t][0]); PLOG("\n");
        PLOG("[paging]   last entry:  "); PLOG_HEX32(boot_page_tables[t][PAGE_ENTRIES - 1]); PLOG("\n");
    }
    PLOG("[paging] identity tables built ("); PLOG_HEX32(PAGING_BOOT_IDENTITY_TABLES); PLOG(" tables, ");
    PLOG_HEX32(PAGING_BOOT_IDENTITY_TABLES * 4096u * 1024u); PLOG(" bytes identity-mapped)\n");

    PLOG("[paging] cr0 before load: "); PLOG_HEX32(read_cr0()); PLOG("\n");
    PLOG("[paging] cr3 before load: "); PLOG_HEX32(read_cr3()); PLOG("\n");

    PLOG("[paging] loading page directory into CR3...\n");
    load_page_directory(page_directory);
    PLOG("[paging] cr3 after load: "); PLOG_HEX32(read_cr3()); PLOG("\n");

    PLOG("[paging] enabling paging (setting PG bit in CR0)...\n");
    enable_paging();
    PLOG("[paging] cr0 after enable: "); PLOG_HEX32(read_cr0()); PLOG("\n");

    PLOG("[paging] paging_init: done\n");
}

int paging_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    unsigned int pd_index = virt_addr >> 22;
    unsigned int pt_index = (virt_addr >> 12) & 0x3FFu;

    PLOG("[paging] map_page: virt="); PLOG_HEX32(virt_addr);
    PLOG(" phys="); PLOG_HEX32(phys_addr);
    PLOG(" flags="); PLOG_HEX32(flags);
    PLOG(" pd_index="); PLOG_HEX32(pd_index);
    PLOG(" pt_index="); PLOG_HEX32(pt_index);
    PLOG("\n");

    uint32_t *table;

    if (page_directory[pd_index] & PAGE_PRESENT) {
        table = (uint32_t *)(page_directory[pd_index] & 0xFFFFF000u);
        PLOG("[paging]   pd entry present, table @ "); PLOG_HEX32((uint32_t)table); PLOG("\n");
    } else {
        PLOG("[paging]   pd entry not present, allocating new table via pmm_alloc_block()\n");
        void *new_table = pmm_alloc_block();
        if (!new_table) {
            PLOG("[paging]   pmm_alloc_block() FAILED -- out of memory\n");
            return -1;
        }

        table = (uint32_t *)new_table;
        PLOG("[paging]   new table @ "); PLOG_HEX32((uint32_t)table); PLOG("\n");
        for (unsigned int i = 0; i < PAGE_ENTRIES; i++) table[i] = 0;

        page_directory[pd_index] = ((uint32_t)table) | PAGE_PRESENT | PAGE_RW;
        PLOG("[paging]   pd["); PLOG_HEX32(pd_index); PLOG("] = "); PLOG_HEX32(page_directory[pd_index]); PLOG("\n");
    }

    table[pt_index] = (phys_addr & 0xFFFFF000u) | (flags & 0xFFFu) | PAGE_PRESENT;
    PLOG("[paging]   pt["); PLOG_HEX32(pt_index); PLOG("] = "); PLOG_HEX32(table[pt_index]); PLOG("\n");

    __asm__ __volatile__("invlpg (%0)" : : "r"(virt_addr) : "memory");
    PLOG("[paging]   invlpg "); PLOG_HEX32(virt_addr); PLOG(" done\n");
    PLOG("[paging] map_page: success\n");
    return 0;
}

void paging_unmap_page(uint32_t virt_addr) {
    unsigned int pd_index = virt_addr >> 22;
    unsigned int pt_index = (virt_addr >> 12) & 0x3FFu;

    PLOG("[paging] unmap_page: virt="); PLOG_HEX32(virt_addr);
    PLOG(" pd_index="); PLOG_HEX32(pd_index);
    PLOG(" pt_index="); PLOG_HEX32(pt_index);
    PLOG("\n");

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        PLOG("[paging]   pd entry not present, nothing to unmap\n");
        return;
    }

    uint32_t *table = (uint32_t *)(page_directory[pd_index] & 0xFFFFF000u);
    PLOG("[paging]   table @ "); PLOG_HEX32((uint32_t)table); PLOG("\n");
    PLOG("[paging]   clearing pt["); PLOG_HEX32(pt_index); PLOG("] (was "); PLOG_HEX32(table[pt_index]); PLOG(")\n");
    table[pt_index] = 0;

    __asm__ __volatile__("invlpg (%0)" : : "r"(virt_addr) : "memory");
    PLOG("[paging]   invlpg "); PLOG_HEX32(virt_addr); PLOG(" done\n");
    PLOG("[paging] unmap_page: done\n");
}