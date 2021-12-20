#include <kernel/mm.h>
#include <kernel/buddy.h>

struct buddy_allocator phys_allocator;

/** The area of memory reserved for the physical memory allocator.
    In this case, the free block arrays for the buddy allocator. */
uint8_t free_phys_blocks[0x40000];

void *kmalloc(size_t size) {
  return NULL;
}

void *kcalloc(size_t elem_size, size_t n_elems) {
  return NULL;
}

void *krealloc(void *ptr, size_t new_size) {
  return NULL;
}

void kfree(void *ptr) {

}
