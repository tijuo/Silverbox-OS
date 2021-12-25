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
#include <kernel/types/list.h>

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

typedef int (*kmem_cache_ctor_t)(void *, size_t);
typedef int (*kmem_cache_dtor_t)(void *, size_t);

struct kslab_node {
  /**
    Slab node list node

    The next and prev point to the next and previous slab nodes in the
    cache. Item points to the start of the slab.
  */
  list_node_t node;

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
      Cache color

      There are '1 + (slab_size % sizeof(buf_size)) / word_size',
      where word_size = sizeof (uint64_t) for 64-bit machines.
  */

  uint8_t color;
};

struct kfree_buf_node {
  struct kfree_buf_node *next;
  struct kslab_node *slab_node;
  void *buffer;
  uint64_t checksum;
};

struct kmem_cache {
  list_node_t node;
  list_t free_slabs;
  list_t partially_used_slabs;
  list_t used_slabs;

  const char *name;
  size_t size;
  size_t align;

  kmem_cache_ctor_t constructor;
  kmem_cache_dtor_t destructor;
};

/**
  Allocate a single 4 KiB physical frame.

  @return The address of the physical frame, on success. NULL, on failure.
*/

void *kalloc_page(void);

/**
  Releases a sigle 4 KiB physical frame.

  @param addr The starting address of the physical frame.
*/

void kfree_page(void *addr);

/**
  Calculates the number of cache colors for a slab.

  @param mem_cache The memory cache.
  @param slab_node The slab node.
  @return E_OK, on success. E_FAIL, on failure.
*/

NON_NULL_PARAMS PURE static inline size_t kmem_cache_num_colors(const struct kmem_cache *mem_cache, const struct kslab_node *slab_node) {
  return 1ul + (((1ul << slab_node->log2_order) - sizeof(struct kslab_node))
    % (mem_cache->size)) / mem_cache->align;
}

/**
  Initialize and add a memory cache to the list of memory caches.

  @param mem_cache The memory cache.
  @param name A label to describe the memory object.
  @param obj_size The size of a memory object in bytes.
  @param align The desired memory alignment of each memory object in bytes. If align
  is less than 8, it will be set to 8.
  @param ctor The function to be used to construct a memory object. May be NULL if no constructor
  needs to be used.
  @param dtor The function to be used to destroy a memory object. May be NULL if no destructor
  needs to be used.
  @return E_OK, on success. E_FAIL, on failure.
*/

NON_NULL_PARAM(1) int kmem_cache_add(struct kmem_cache *mem_cache, const char *name, size_t obj_size, size_t align,
                    kmem_cache_ctor_t ctor, kmem_cache_dtor_t dtor);

/**
  Locate a memory cache by name.

  @param name The label that was used to register the memory cache.
  @return The memory cache, on success. NULL, on failure.
*/


NON_NULL_PARAM(1) struct kmem_cache *kmem_cache_find(const char *name);

/**
  Initializes a memory object cache.

  @param mem_cache The memory cache.
  @param name A label to describe the memory object.
  @param obj_size The size of a memory object in bytes.
  @param align The desired memory alignment of each memory object in bytes. If align
  is less than 8, it will be set to 8.
  @param ctor The function to be used to construct a memory object. May be NULL if no constructor
  needs to be used.
  @param dtor The function to be used to destroy a memory object. May be NULL if no destructor
  needs to be used.
  @return E_OK, on success. E_FAIL, on failure.
*/

NON_NULL_PARAM(1) int kmem_cache_init(struct kmem_cache *mem_cache, const char *name, size_t obj_size, size_t align,
  kmem_cache_ctor_t ctor, kmem_cache_dtor_t dtor);

/**
  Constructs all memory objects in a slab and adds the slab to its cache's slab list.

  @param mem_cache The memory cache.
  @param slab_node The slab node that will have its objects constructed.
  @return E_OK, on success. E_FAIL, on failure.
*/

int kmem_cache_init_slab(struct kmem_cache *mem_cache, struct kslab_node *slab_node);

/**
  Destroys all memory objects in a slab and unlinks the slab from its cache's slab list.

  @param mem_cache The memory cache.
  @param slab_node The slab node that will have its objects destroyed.
  @return E_OK, on success. E_FAIL, on failure.
*/

NON_NULL_PARAMS int kmem_cache_destroy_slab(struct kmem_cache *mem_cache, struct kslab_node *slab_node);

void *kmem_cache_alloc(struct kmem_cache *mem_cache, int flags);
int kmem_cache_free(struct kmem_cache *mem_cache, void *buf);

void *kmalloc(size_t size, size_t align);
void *kcalloc(size_t count, size_t elem_size, size_t align);
void *krealloc(void *mem, size_t new_size, size_t align);
void kfree(void *mem);

extern struct buddy_allocator phys_allocator;
extern uint8_t free_phys_blocks[0x40000];
extern list_t free_frame_list;
extern struct kmem_cache kbuf_node_mem_cache;
extern list_t kmem_cache_list;

extern struct kmem_cache mem8_mem_cache;
extern struct kmem_cache mem16_mem_cache;
extern struct kmem_cache mem24_mem_cache;
extern struct kmem_cache mem32_mem_cache;
extern struct kmem_cache mem48_mem_cache;
extern struct kmem_cache mem64_mem_cache;
extern struct kmem_cache mem96_mem_cache;
extern struct kmem_cache mem128_mem_cache;
extern struct kmem_cache mem192_mem_cache;
extern struct kmem_cache mem256_mem_cache;
extern struct kmem_cache mem384_mem_cache;
extern struct kmem_cache mem512_mem_cache;

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
