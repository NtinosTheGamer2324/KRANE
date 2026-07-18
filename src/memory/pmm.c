#include "memory/pmm.h"
#include "memory/e820.h"
#include "vga.h"
#include <stdint.h>

#define PLOG(str) log_str(str, 0x07)
#define PLOG_HEX32(val) log_hex_uint32((uint32_t)(val), 0x0E)
#define PLOG_HEX64(val) log_hex_uint64((unsigned long long)(val), 0x0E)

static uint8_t pmm_bitmap[PMM_BITMAP_SIZE];
static unsigned int total_blocks = 0;
static unsigned int used_blocks = 0;

static inline void bitmap_set(unsigned int bit) {
    pmm_bitmap[bit / 8] |= (uint8_t)(1u << (bit % 8));
}

static inline void bitmap_clear(unsigned int bit) {
    pmm_bitmap[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

static inline int bitmap_test(unsigned int bit) {
    return pmm_bitmap[bit / 8] & (uint8_t)(1u << (bit % 8));
}

/* Returns the index of the first run of `count` contiguous free blocks,
 * or -1 if none exist. */
static int bitmap_find_free_run(unsigned int count) {
    unsigned int run = 0;
    unsigned int start = 0;

    for (unsigned int i = 0; i < total_blocks; i++) {
        if (!bitmap_test(i)) {
            if (run == 0) start = i;
            run++;
            if (run == count) return (int)start;
        } else {
            run = 0;
        }
    }
    return -1;
}

void pmm_init(void) {
    e820_entry_t *entries = (e820_entry_t *)E820_ENTRIES_ADDR;
    unsigned int entry_count = *(unsigned int *)E820_COUNT_ADDR;

    PLOG("[pmm] pmm_init: start\n");
    PLOG("[pmm]   E820_ENTRIES_ADDR="); PLOG_HEX32(E820_ENTRIES_ADDR);
    PLOG(" E820_COUNT_ADDR="); PLOG_HEX32(E820_COUNT_ADDR);
    PLOG(" entry_count="); PLOG_HEX32(entry_count); PLOG("\n");
    PLOG("[pmm]   PMM_MAX_MEMORY="); PLOG_HEX32(PMM_MAX_MEMORY);
    PLOG(" PMM_MAX_BLOCKS="); PLOG_HEX32(PMM_MAX_BLOCKS);
    PLOG(" PMM_BITMAP_SIZE="); PLOG_HEX32(PMM_BITMAP_SIZE);
    PLOG(" bitmap@"); PLOG_HEX32((uint32_t)pmm_bitmap); PLOG("\n");

    total_blocks = PMM_MAX_BLOCKS;

    /* Start with everything marked used; we only free what E820 reports
     * as USABLE. This is safer than the inverse if E820 ever comes back
     * incomplete. */
    PLOG("[pmm]   marking entire bitmap as used\n");
    for (unsigned int i = 0; i < PMM_BITMAP_SIZE; i++) pmm_bitmap[i] = 0xFF;
    used_blocks = total_blocks;

    for (unsigned int i = 0; i < entry_count; i++) {
        PLOG("[pmm]   e820["); PLOG_HEX32(i); PLOG("] type="); PLOG_HEX32(entries[i].type);
        PLOG(" base="); PLOG_HEX64(entries[i].base_addr);
        PLOG(" len="); PLOG_HEX64(entries[i].length); PLOG("\n");

        if (entries[i].type != E820_TYPE_USABLE) {
            PLOG("[pmm]     skipping (not usable)\n");
            continue;
        }

        uint64_t base = entries[i].base_addr;
        uint64_t end = base + entries[i].length;

        if (base >= PMM_MAX_MEMORY) {
            PLOG("[pmm]     skipping (base >= PMM_MAX_MEMORY)\n");
            continue;
        }
        if (end > PMM_MAX_MEMORY) {
            PLOG("[pmm]     clamping end to PMM_MAX_MEMORY\n");
            end = PMM_MAX_MEMORY;
        }

        /* Round base up and end down to block boundaries so we never
         * free a partial block. */
        uint64_t aligned_base = (base + PMM_BLOCK_SIZE - 1) & ~((uint64_t)PMM_BLOCK_SIZE - 1);
        uint64_t aligned_end  = end & ~((uint64_t)PMM_BLOCK_SIZE - 1);

        PLOG("[pmm]     aligned_base="); PLOG_HEX64(aligned_base);
        PLOG(" aligned_end="); PLOG_HEX64(aligned_end); PLOG("\n");

        unsigned int freed_here = 0;
        for (uint64_t addr = aligned_base; addr < aligned_end; addr += PMM_BLOCK_SIZE) {
            unsigned int block = (unsigned int)(addr / PMM_BLOCK_SIZE);
            if (bitmap_test(block)) {
                bitmap_clear(block);
                used_blocks--;
                freed_here++;
            }
        }
        PLOG("[pmm]     freed "); PLOG_HEX32(freed_here); PLOG(" blocks from this entry\n");
    }

    /* Unconditionally reserve the low 1MB: real-mode IVT/BDA, the
     * bootloader stages, the stack, and the E820 table itself all live
     * down here regardless of what E820 says. */
    unsigned int low_blocks = 0x100000u / PMM_BLOCK_SIZE;
    PLOG("[pmm]   reserving low 1MB ("); PLOG_HEX32(low_blocks); PLOG(" blocks)\n");
    for (unsigned int b = 0; b < low_blocks && b < total_blocks; b++) {
        if (!bitmap_test(b)) {
            bitmap_set(b);
            used_blocks++;
        }
    }

    PLOG("[pmm] pmm_init: done. total="); PLOG_HEX32(total_blocks);
    PLOG(" used="); PLOG_HEX32(used_blocks);
    PLOG(" free="); PLOG_HEX32(total_blocks - used_blocks); PLOG("\n");
}

void *pmm_alloc_block(void) {
    int idx = bitmap_find_free_run(1);
    if (idx < 0) {
        PLOG("[pmm] alloc_block: FAILED (no free blocks)\n");
        return (void *)0;
    }

    bitmap_set((unsigned int)idx);
    used_blocks++;
    void *addr = (void *)((uint32_t)idx * PMM_BLOCK_SIZE);
    PLOG("[pmm] alloc_block: idx="); PLOG_HEX32(idx); PLOG(" addr="); PLOG_HEX32((uint32_t)addr);
    PLOG(" used="); PLOG_HEX32(used_blocks); PLOG("\n");
    return addr;
}

void *pmm_alloc_blocks(unsigned int count) {
    if (count == 0) return (void *)0;

    int idx = bitmap_find_free_run(count);
    if (idx < 0) {
        PLOG("[pmm] alloc_blocks: FAILED count="); PLOG_HEX32(count); PLOG(" (no free run)\n");
        return (void *)0;
    }

    for (unsigned int i = 0; i < count; i++) bitmap_set((unsigned int)idx + i);
    used_blocks += count;
    void *addr = (void *)((uint32_t)idx * PMM_BLOCK_SIZE);
    PLOG("[pmm] alloc_blocks: idx="); PLOG_HEX32(idx); PLOG(" count="); PLOG_HEX32(count);
    PLOG(" addr="); PLOG_HEX32((uint32_t)addr); PLOG(" used="); PLOG_HEX32(used_blocks); PLOG("\n");
    return addr;
}

void pmm_free_block(void *addr) {
    unsigned int block = (unsigned int)((uint32_t)addr / PMM_BLOCK_SIZE);
    PLOG("[pmm] free_block: addr="); PLOG_HEX32((uint32_t)addr); PLOG(" block="); PLOG_HEX32(block); PLOG("\n");
    if (block >= total_blocks) {
        PLOG("[pmm]   block out of range, ignoring\n");
        return;
    }

    if (bitmap_test(block)) {
        bitmap_clear(block);
        used_blocks--;
        PLOG("[pmm]   freed. used="); PLOG_HEX32(used_blocks); PLOG("\n");
    } else {
        PLOG("[pmm]   block already free, ignoring\n");
    }
}

void pmm_free_blocks(void *addr, unsigned int count) {
    unsigned int block = (unsigned int)((uint32_t)addr / PMM_BLOCK_SIZE);
    PLOG("[pmm] free_blocks: addr="); PLOG_HEX32((uint32_t)addr); PLOG(" block="); PLOG_HEX32(block);
    PLOG(" count="); PLOG_HEX32(count); PLOG("\n");

    for (unsigned int i = 0; i < count; i++) {
        if (block + i >= total_blocks) {
            PLOG("[pmm]   reached out-of-range block, stopping early\n");
            break;
        }
        if (bitmap_test(block + i)) {
            bitmap_clear(block + i);
            used_blocks--;
        }
    }
    PLOG("[pmm]   freed. used="); PLOG_HEX32(used_blocks); PLOG("\n");
}

unsigned int pmm_get_total_block_count(void) { return total_blocks; }
unsigned int pmm_get_used_block_count(void)  { return used_blocks; }
unsigned int pmm_get_free_block_count(void)  { return total_blocks - used_blocks; }