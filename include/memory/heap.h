#ifndef HEAP_H
#define HEAP_H

/* Must be called after paging_init(), since the heap arena lives inside
 * the boot identity-mapped region. */
void heap_init(void);

void *kmalloc(unsigned int size);
void  kfree(void *ptr);

unsigned int heap_get_used_bytes(void);
unsigned int heap_get_free_bytes(void);

#endif // HEAP_H