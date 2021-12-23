#ifndef MM_H
#define MM_H

#include <stdbool.h>
#include <util.h>
#include <kernel/lowlevel.h>
#include <kernel/pae.h>
#include <kernel/error.h>
#include <types.h>
#include <stddef.h>
#include <kernel/buddy.h>

/* FIXME: Changing any of these values may require changing
 the asm code */

#define KERNEL_TCB_START	((addr_t)&ktcb_start)
#define KERNEL_TCB_END		((addr_t)&ktcb_end)
#define KERNEL_VSTART		((addr_t)&kvirt_start)
#define KERNEL_VEND			((addr_t)&kend)

#define KERNEL_PHYS_START	((addr_t)&kvirt_low_mem_start)

#define INVALID_VADDR       ((addr_t)0xFFFFFFFFFFFFFFFFul)
#define INVALID_ADDR        ((addr_t)0xFFFFFFFFFFFFFFFFul)

#define CURRENT_ROOT_PMAP	(paddr_t)0xFFFFFFFFFFFFFFFFul

/** Aligns an address to the previous boundary (if not already aligned) */
#define ALIGN_DOWN(addr, boundary)	((addr_t)((addr) & ~((boundary) - 1) ))

#define IS_ALIGNED(addr, boundary)	(((addr) & ((boundary) - 1)) == 0)

/** Aligns an address to next boundary (even if it's already aligned) */

#define ALIGN_NEXT(addr, boundary)	(ALIGN_DOWN(addr, boundary) + boundary)

/// Aligns an address to the next boundary (if not already aligned)
#define ALIGN_UP(addr, boundary) \
({ \
  __typeof__ (boundary) _boundary=(boundary);  \
  __typeof__ (addr) _addr=(addr);              \
  (_addr == ALIGN_DOWN(_addr, _boundary) ?     \
    _addr : ALIGN_NEXT(_addr, _boundary)); \
})

#define ALIGN(addr, boundary)         ALIGN_UP(addr, boundary)

int initialize_root_pmap(paddr_t root_pmap);

NON_NULL_PARAMS HOT
int peek_virt(addr_t address, size_t len, void *buffer, paddr_t addr_space);

NON_NULL_PARAMS HOT
int poke_virt(addr_t address, size_t len, void *buffer, paddr_t addr_space);

/**
 Can the kernel perform some memory access at some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param root_pmap The physical address of the address space.
 @param is_read_only true if a read-only access. false for write or read/write access.
 @return true if address is accessible. false, otherwise.
 **/

//bool is_accessible(addr_t addr, paddr_t root_pmap, bool is_read_only);

/**
 Can the kernel read data from some virtual address in a particular address space?

 @param addr The virtual address to be tested.
 @param root_pmap The physical address of the address space
 @return true if address is readable. false, otherwise.
 **/

static inline bool is_readable(addr_t addr, paddr_t root_pmap) {
  paddr_t paddr;
  return !IS_ERROR(translate_vaddr(addr, &paddr, root_pmap));
}

#define SLAB_FREE_BUF_END   65535u

typedef int (*kmem_cache_ctor)(void *, size_t);
typedef int (*kmem_cache_dtor)(void *, size_t);

struct kslab_node {
  /**
    Slab node list node

    The next and prev point to the next and previous slab nodes in the
    cache. Item points to the start of the slab.
  */
  list_node_t node;

  /**
    The offset of the first free_buf_node or small_buff_node from the slab start.
    Only valid if status is free or partially used.
  */

  uint16_t free_head_offset;

  /**
    Reference counter for the slab. Every time a buffer is allocated,
    the reference count is incremented by one. The count is decremented
    by one upon freeing. When the reference count is set to zero, then
    the entire slab can be marked as free or destroyed (if memory is needed).
  */

  uint16_t ref_count;
  uint8_t lock;

  /**
    The base-2 logarithm of the slab size.
  */

  uint8_t log2_order;

  /**
    Slab status: free, partially used, or used
  */

  uint8_t status;

  /**
      Cache color

      There are '1 + (slab_size % sizeof(buf_size)) / word_size',
      where word_size = sizeof (uint64_t) for 64-bit machines.
  */

  uint8_t color;
};

struct free_buf_node {
  uint16_t next_offset;
  /**
    Checksum

    next_offset + checksum must equal zero.
  */
  uint16_t checksum;
};

struct kmem_cache {
  list_t free_slabs;
  list_t partially_used_slabs;
  list_t used_slabs;

  int obj_type;
  size_t size;
  size_t align;

  kmem_cache_ctor constructor;
  kmem_cache_dtor destructor;
};

NON_NULL_PARAMS PURE size_t kmem_cache_num_colors(const struct kmem_cache *mem_cache, const struct kslab_node *slab_node) {
  return 1ul + (((1ul << slab_node->log2_order) - sizeof(struct kslab_node))
    % (mem_cache->size) + sizeof(void *)) / mem_cache->align;
}

NON_NULL_PARAM(1) int kmem_cache_init(struct kmem_cache *mem_cache, int obj_type, size_t obj_size, size_t align,
  kmem_cache_ctor_t ctor, kmem_cache_dtor_t dtor);

void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t elem_size);
void *krealloc(void *mem, size_t new_size);
void kfree(void *mem);

extern struct buddy_allocator phys_allocator;
extern uint8_t free_phys_blocks[0x40000];

#if 0

#define KERNEL_PHYS_START	((addr_t)&kvirt_low_mem_start)
#define KERNEL_START		((addr_t)&kPhysStart)
#define RESD_PHYSMEM	    KERNEL_START

/* FIXME: Changing any of these values may require changing the
 asm code. */

#define KMAP_AREA			(0xFF800000u)
#define KMAP_AREA2			(0xFFC00000u)
#define IOAPIC_VADDR		KMAP_AREA2
#define LAPIC_VADDR         (IOAPIC_VADDR + 0x100000u)
#define TEMP_PAGE			(KMAP_AREA2 + 0x3FF000u)

#endif
#endif /* MM_H */
