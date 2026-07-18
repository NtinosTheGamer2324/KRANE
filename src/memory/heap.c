#include "memory/heap.h"
#include "vga.h"
#include <stdint.h>

#define HLOG(str) log_str(str, 0x07)
#define HLOG_HEX32(val) log_hex_uint32((uint32_t)(val), 0x0E)

/* Fixed arena inside the boot identity-mapped region (0-16MB), well
 * clear of the bootloader image, the E820 table, and the PMM bitmap. */
#define HEAP_START 0x00800000u  /* 8MB */
#define HEAP_SIZE  0x00400000u  /* 4MB */
#define HEAP_END   (HEAP_START + HEAP_SIZE)

#define HEAP_ALIGN 8u
#define HEAP_MAGIC 0xA11C0DEu

typedef struct block_header {
    unsigned int magic;
    unsigned int size;        /* usable bytes following this header */
    int free;
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

static block_header_t *heap_base = (block_header_t *)0;

static inline unsigned int align_up(unsigned int n, unsigned int a) {
    return (n + (a - 1)) & ~(a - 1);
}

void heap_init(void) {
    HLOG("[heap] heap_init: start. HEAP_START="); HLOG_HEX32(HEAP_START);
    HLOG(" HEAP_SIZE="); HLOG_HEX32(HEAP_SIZE);
    HLOG(" HEAP_END="); HLOG_HEX32(HEAP_END); HLOG("\n");

    heap_base = (block_header_t *)HEAP_START;
    heap_base->magic = HEAP_MAGIC;
    heap_base->size = HEAP_SIZE - sizeof(block_header_t);
    heap_base->free = 1;
    heap_base->next = (block_header_t *)0;
    heap_base->prev = (block_header_t *)0;

    HLOG("[heap]   heap_base @ "); HLOG_HEX32((uint32_t)heap_base);
    HLOG(" usable_size="); HLOG_HEX32(heap_base->size); HLOG("\n");
    HLOG("[heap] heap_init: done\n");
}

/* Splits `block` if it has enough spare room to host another header
 * plus at least HEAP_ALIGN bytes, so we don't hand out more memory
 * than was asked for. */
static void split_block(block_header_t *block, unsigned int size) {
    unsigned int remaining = block->size - size;

    if (remaining <= sizeof(block_header_t) + HEAP_ALIGN) return;

    block_header_t *new_block = (block_header_t *)((uint8_t *)(block + 1) + size);
    new_block->magic = HEAP_MAGIC;
    new_block->size = remaining - sizeof(block_header_t);
    new_block->free = 1;
    new_block->next = block->next;
    new_block->prev = block;

    if (block->next) block->next->prev = new_block;
    block->next = new_block;
    block->size = size;
}

static void coalesce_forward(block_header_t *block) {
    while (block->next && block->next->free) {
        block_header_t *next = block->next;
        block->size += sizeof(block_header_t) + next->size;
        block->next = next->next;
        if (next->next) next->next->prev = block;
    }
}

void *kmalloc(unsigned int size) {
    HLOG("[heap] kmalloc(size="); HLOG_HEX32(size); HLOG(")\n");

    if (!heap_base || size == 0) {
        HLOG("[heap]   kmalloc FAILED (no heap_base or size==0)\n");
        return (void *)0;
    }

    size = align_up(size, HEAP_ALIGN);

    for (block_header_t *block = heap_base; block; block = block->next) {
        if (block->free && block->size >= size) {
            split_block(block, size);
            block->free = 0;
            HLOG("[heap]   allocated @ "); HLOG_HEX32((uint32_t)(block + 1));
            HLOG(" (block hdr @ "); HLOG_HEX32((uint32_t)block); HLOG(")\n");
            return (void *)(block + 1);
        }
    }

    HLOG("[heap]   kmalloc FAILED (out of heap space)\n");
    return (void *)0; /* out of heap space */
}

void kfree(void *ptr) {
    HLOG("[heap] kfree(ptr="); HLOG_HEX32((uint32_t)ptr); HLOG(")\n");

    if (!ptr) {
        HLOG("[heap]   kfree no-op (null ptr)\n");
        return;
    }

    block_header_t *block = (block_header_t *)ptr - 1;
    if (block->magic != HEAP_MAGIC) {
        HLOG("[heap]   kfree FAILED (bad magic, corrupted or foreign pointer)\n");
        return; /* not one of ours / corrupted */
    }

    block->free = 1;

    /* Coalesce with the following free block(s) first, then merge
     * backwards by walking the chain from the block before us. */
    coalesce_forward(block);
    if (block->prev && block->prev->free) {
        coalesce_forward(block->prev);
    }
    HLOG("[heap]   freed.\n");
}

unsigned int heap_get_used_bytes(void) {
    unsigned int used = 0;
    for (block_header_t *block = heap_base; block; block = block->next) {
        if (!block->free) used += block->size;
    }
    return used;
}

unsigned int heap_get_free_bytes(void) {
    unsigned int free_bytes = 0;
    for (block_header_t *block = heap_base; block; block = block->next) {
        if (block->free) free_bytes += block->size;
    }
    return free_bytes;
}