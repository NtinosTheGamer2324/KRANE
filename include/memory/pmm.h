#ifndef PMM_H
#define PMM_H

#define PMM_BLOCK_SIZE   4096u
#define PMM_MAX_MEMORY   (256u * 1024u * 1024u)              /* 256MB ceiling for the bitmap */
#define PMM_MAX_BLOCKS   (PMM_MAX_MEMORY / PMM_BLOCK_SIZE)
#define PMM_BITMAP_SIZE  (PMM_MAX_BLOCKS / 8u)

/* Parses the E820 map left at 0x500/0x508 by stage2 and builds the
 * physical frame bitmap. Anything below 1MB is always reserved. */
void pmm_init(void);

void *pmm_alloc_block(void);
void *pmm_alloc_blocks(unsigned int count);
void  pmm_free_block(void *addr);
void  pmm_free_blocks(void *addr, unsigned int count);

unsigned int pmm_get_total_block_count(void);
unsigned int pmm_get_used_block_count(void);
unsigned int pmm_get_free_block_count(void);

#endif // PMM_H