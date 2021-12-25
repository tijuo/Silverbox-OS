#include <kernel/mm.h>
#include <kernel/buddy.h>
#include <kernel/debug.h>
#include <x86gprintrin.h>
#include <kernel/memory.h>

#define SLAB_SIZE   PAGE_SIZE

#define GET_KSLAB_NODE(node)        ((struct kslab_node *)(node))
#define FREE_BUF_HEAD(slab_node)    ((struct kfree_buf_node *)((slab_node)->node.item.item_void_ptr))
#define SET_FREE_BUF_HEAD(slab_node, n)  ((slab_node)->node.item.item_void_ptr = (n))
#define BUF_BITMAP(slab_node)       (slab_node)->node.item.item_uint64_t

#define MIN_OBJ_SIZE      sizeof(long int)
#define MIN_ALIGN         sizeof(long int)
#define COLOR_SIZE        sizeof(long int)

NON_NULL_PARAMS static inline bool is_checksum_valid(const struct kfree_buf_node *node)
{
  return (uintptr_t)node->buffer + (uintptr_t)node->next + (uintptr_t)node->slab_node + node->checksum == 0;
}

list_t free_frame_list;
//list_t dma_free_frame_list;

struct buddy_allocator phys_allocator;

/** The area of memory reserved for the physical memory allocator.
    In this case, the free block arrays for the buddy allocator. */
uint8_t free_phys_blocks[0x40000];

struct kmem_cache kbuf_node_mem_cache;
list_t kmem_cache_list;

struct kmem_cache mem8_mem_cache;
struct kmem_cache mem16_mem_cache;
struct kmem_cache mem24_mem_cache;
struct kmem_cache mem32_mem_cache;
struct kmem_cache mem48_mem_cache;
struct kmem_cache mem64_mem_cache;
struct kmem_cache mem96_mem_cache;
struct kmem_cache mem128_mem_cache;
struct kmem_cache mem192_mem_cache;
struct kmem_cache mem256_mem_cache;
struct kmem_cache mem384_mem_cache;
struct kmem_cache mem512_mem_cache;

/**
  Initializes a slab.

  @param start The starting address of the slab.
  @param slab_size The size of the slab in bytes.
*/

static NON_NULL_PARAMS struct kslab_node *kslab_init(void *start, size_t slab_size);

int kfree_buf_node_constructor(void *buf, size_t size);

int kfree_buf_node_constructor(void *buf, size_t size)
{
  struct kfree_buf_node *node = (struct kfree_buf_node *)buf;

  node->next = NULL;
  node->slab_node = NULL;
  node->buffer = NULL;
  node->checksum = 0;

  kassert(is_checksum_valid(node) == true);

  return E_OK;
}

int kmem_cache_add(struct kmem_cache *mem_cache, const char *name, size_t obj_size, size_t align,
                   kmem_cache_ctor_t ctor, kmem_cache_dtor_t dtor)
{
  if(IS_ERROR(kmem_cache_init(mem_cache, name, obj_size, align, ctor, dtor)))
    RET_MSG(E_FAIL, "Unable to add memory cache.");

  list_insert_node_at_end(&kmem_cache_list, true, &mem_cache->node);

  return E_OK;
}

struct kmem_cache *kmem_cache_find(const char *name)
{
  for(const list_node_t *node = kmem_cache_list.head; node; node = node->next) {
    const struct kmem_cache *mem_cache = (struct kmem_cache *)node->item.item_void_ptr;

    if(kstrcmp(name, mem_cache->name) == 0)
      return mem_cache;
  }

  return NULL;
}

void *kalloc_page(void)
{
  list_node_t *node;

  if(IS_ERROR(list_remove_node_from_end(&free_frame_list, true, &node)))
    RET_MSG(NULL, "Unable to remove node from free list.");

  kassert((uintptr_t)node->item.item_void_ptr - (uintptr_t)node >= PAGE_SIZE);
  kassert(node->item.item_void_ptr != NULL);

  if((uintptr_t)node->item.item_void_ptr - (uintptr_t)node > PAGE_SIZE) {
    list_node_t *new_node = (list_node_t *)((uintptr_t)node + PAGE_SIZE);

    new_node->item.item_void_ptr = node->item.item_void_ptr;
    list_insert_node_at_end(&free_frame_list, true, new_node);
  }

  return node;
}

void kfree_page(void *addr)
{
  kassert(((uintptr_t)addr & (PAGE_SIZE - 1)) == 0);

  list_node_t *node = (list_node_t *)addr;

  node->item.item_void_ptr = (void *)((uintptr_t)node + PAGE_SIZE);
  list_insert_node_at_end(&free_frame_list, true, node);
}

struct kslab_node *kslab_init(void *start, size_t slab_size)
{
  struct kslab_node *slab_node = (struct kslab_node *)start;

  if(slab_size == 0)
    RET_MSG(NULL, "Slab size cannot be zero.");

  kmemset(slab_node, 0, sizeof * slab_node);

  slab_node->log2_order = __bsrq(slab_size);

  if(__bsfq(slab_size) != slab_node->log2_order)
    slab_node->log2_order++;

  return slab_node;
}

int kmem_cache_init(struct kmem_cache *mem_cache, const char *name, size_t obj_size, size_t align,
                    kmem_cache_ctor_t ctor, kmem_cache_dtor_t dtor)
{
  if(obj_size < MIN_OBJ_SIZE)
    obj_size = MIN_OBJ_SIZE;

  if(align < MIN_ALIGN)
    align = MIN_ALIGN;

  mem_cache->name = name;
  mem_cache->size = obj_size;
  mem_cache->align = align;
  mem_cache->constructor = ctor;
  mem_cache->destructor = dtor;
  mem_cache->node.item.item_void_ptr = mem_cache;
  mem_cache->node.next = NULL;
  mem_cache->node.prev = NULL;

  list_init(&mem_cache->free_slabs);
  list_init(&mem_cache->partially_used_slabs);
  list_init(&mem_cache->used_slabs);

  return E_OK;
}

/* After the free slab has been initialized, memory objects are ready to be allocated.
   Simply return an object from the free list. */

int kmem_cache_init_slab(struct kmem_cache *mem_cache, struct kslab_node *slab_node)
{
  // Set the slab's color by incrementing an existing slab's color by one.

  const list_t *slab_lists[3] = { &mem_cache->free_slabs, &mem_cache->partially_used_slabs, &mem_cache->used_slabs };

  for(size_t i = 0; i < 3; i++) {
    if(!is_list_empty(slab_lists[i])) {
      const struct kslab_node *cache_slab_node = GET_KSLAB_NODE(list_peek_tail(slab_lists[i]));

      slab_node->color = (uint8_t)((cache_slab_node->color + 1)
                                   % kmem_cache_num_colors(mem_cache, slab_node));
    }
  }

  slab_node->ref_count = 0;

  size_t offset = sizeof * slab_node + COLOR_SIZE * slab_node->color;

  for(; offset < (1ul << slab_node->log2_order); offset += mem_cache->size) {
    if(mem_cache->constructor)
      mem_cache->constructor((void *)((uintptr_t)slab_node + offset), mem_cache->size);

    if((1ul << slab_node->log2_order) > PAGE_SIZE || mem_cache->size > PAGE_SIZE / 8) {
      struct kfree_buf_node *buf_node = kmem_cache_alloc(&kbuf_node_mem_cache, 0);

      if(buf_node == NULL)
        RET_MSG(E_FAIL, "Unable to allocate kfree_buf_node");

      buf_node->buffer = (void *)((uintptr_t)slab_node + offset);
      buf_node->next = FREE_BUF_HEAD(slab_node);
      buf_node->slab_node = slab_node;
      buf_node->checksum = -((uintptr_t)buf_node->buffer + (uintptr_t)buf_node->next + (uintptr_t)buf_node->slab_node);

      SET_FREE_BUF_HEAD(slab_node, buf_node);
    } else
      BUF_BITMAP(slab_node) = 0;
  }

  list_insert_node_at_end(&mem_cache->free_slabs, true, &slab_node->node);

  return E_OK;
}

void *kmem_cache_alloc(struct kmem_cache *mem_cache, int flags)
{
  struct kslab_node *slab_node = NULL;
  void *ret_buf;

  list_t *slab_lists[2] = { &mem_cache->partially_used_slabs, &mem_cache->free_slabs };

  for(size_t i = 0; i < 2; i++) {
    if(!is_list_empty(slab_lists[i])) {
      list_node_t *list_node;

      if(IS_ERROR(list_remove_node_from_end(slab_lists[i], true, &list_node)))
        RET_MSG(NULL, "Unable to remove slab from partially used slabs list");

      slab_node = GET_KSLAB_NODE(list_node);
      break;
    }
  }

  // todo: v This might need to be placed in another function

  if(slab_node == NULL) {
    void *new_page = kalloc_page();

    if(new_page == NULL)
      RET_MSG(NULL, "Unable to allocate new page.");

    slab_node = kslab_init(new_page, PAGE_SIZE);
    list_insert_node_at_end(&mem_cache->free_slabs, true, &slab_node->node);
  }

  size_t buf_count = ((1ul << slab_node->log2_order) - sizeof * slab_node
                      - COLOR_SIZE * slab_node->color) / mem_cache->size;

  // todo: ^ end

  if(mem_cache->size <= PAGE_SIZE / 8) {
    kassert(BUF_BITMAP(slab_node) != (1ul << buf_count) - 1);

    size_t free_index = (size_t)__bsfq(~BUF_BITMAP(slab_node));

    kassert(free_index < buf_count);

    slab_node->ref_count++;
    BUF_BITMAP(slab_node) |= (1ul << free_index);

    ret_buf = (void *)((uintptr_t)slab_node + sizeof * slab_node
                       + free_index * mem_cache->size + sizeof(uint64_t) * slab_node->color);
  } else {
    struct kfree_buf_node *buf_node = FREE_BUF_HEAD(slab_node);
    const void *buf = buf_node->buffer;

    kassert(buf_node != NULL);

    if((uintptr_t)buf_node->buffer + (uintptr_t)buf_node->next + (uintptr_t)buf_node->slab_node + buf_node->checksum != 0)
      RET_MSG(NULL, "Memory corruption detected in slab's free buffer list.");

    slab_node->ref_count++;
    SET_FREE_BUF_HEAD(slab_node, buf_node->next);
    kmem_cache_free(&kbuf_node_mem_cache, buf_node);

    ret_buf = buf;
  }

  kassert(slab_node->ref_count <= buf_count);

  if(slab_node->ref_count == buf_count) {
    if(slab_node->ref_count == 1) {
      list_unlink_node(slab_node->ref_count == 1
                       ? &mem_cache->free_slabs
                       : &mem_cache->partially_used_slabs,
                       &slab_node->node);

      list_insert_node_at_end(&mem_cache->used_slabs, true, &slab_node->node);
    }
  }
  else if(slab_node->ref_count == 1) {
    list_unlink_node(&mem_cache->free_slabs, &slab_node->node);
    list_insert_node_at_end(&mem_cache->partially_used_slabs, true, &slab_node->node);
  }

  return ret_buf;
}

int kmem_cache_free(struct kmem_cache *mem_cache, void *buf)
{
  struct kslab_node *slab_node = (struct kslab_node *)((uintptr_t)buf & ~(SLAB_SIZE - 1));

  if(mem_cache->size <= PAGE_SIZE / 8) {
    size_t buf_count = ((1ul << slab_node->log2_order) - sizeof * slab_node
                        - sizeof(uint64_t) * slab_node->color) / mem_cache->size;
    size_t free_index = ((uintptr_t)buf - (uintptr_t)slab_node - sizeof * slab_node
                         - sizeof(uint64_t) * slab_node->color) / mem_cache->size;

    // Move a used slab to the partially used slab sublist

    if(BUF_BITMAP(slab_node) == (1ul << buf_count) - 1 && slab_node->ref_count > 1) {
      list_unlink_node(&mem_cache->used_slabs, &slab_node->node);
      list_insert_node_at_end(&mem_cache->partially_used_slabs, true, &slab_node->node);
    }

    BUF_BITMAP(slab_node) &= ~(1ul << free_index);

    #if DEBUG
    if(slab_node->ref_count == 1)
      kassert(BUF_BITMAP(slab_node) == 0);
    #endif /* DEBUG */
  } else {
    struct kfree_buf_node *buf_node = kmem_cache_alloc(&kbuf_node_mem_cache, 0);

    if(buf_node == NULL)
      RET_MSG(E_FAIL, "Unable to allocate kfree_buf_node");

    buf_node->buffer = buf;
    buf_node->next = FREE_BUF_HEAD(slab_node);
    buf_node->slab_node = slab_node;
    buf_node->checksum = -((uintptr_t)buf_node->buffer + (uintptr_t)buf_node->next + (uintptr_t)buf_node->slab_node);

    SET_FREE_BUF_HEAD(slab_node, buf_node);
    kassert(is_checksum_valid(buf_node) == true);
  }

  if(--slab_node->ref_count == 0) {
    list_unlink_node(&mem_cache->partially_used_slabs, &slab_node->node);
    list_insert_node_at_end(&mem_cache->free_slabs, true, &slab_node->node);
  }

  return E_OK;
}

int kmem_cache_destroy_slab(struct kmem_cache *mem_cache, struct kslab_node *slab_node)
{
  struct kfree_buf_node *next;

  if((1ul << slab_node->log2_order) > PAGE_SIZE || mem_cache->size > PAGE_SIZE / 8) {
    for(struct kfree_buf_node *buf_node = FREE_BUF_HEAD(slab_node); buf_node; buf_node = next) {
      if(!is_checksum_valid(buf_node))
        RET_MSG(E_FAIL, "Memory corruption detected in slab's free buffer list.");

      next = buf_node->next;

      if(mem_cache->destructor)
        mem_cache->destructor(buf_node->buffer, mem_cache->size);

      kmem_cache_free(&kbuf_node_mem_cache, buf_node);
    }
  } else {
    if(mem_cache->destructor) {
      size_t offset = sizeof * slab_node + COLOR_SIZE * slab_node->color;

      for(; offset < (1ul << slab_node->log2_order); offset += mem_cache->size) {
        mem_cache->destructor((void *)((uintptr_t)slab_node + offset), mem_cache->size);
      }
    }
  }

  return E_OK;
}


void *kmalloc(size_t size, size_t align)
{
  struct kmem_cache *caches[12] = { &mem8_mem_cache, &mem16_mem_cache, &mem24_mem_cache,
           &mem32_mem_cache, &mem48_mem_cache, &mem64_mem_cache, &mem96_mem_cache,
           &mem128_mem_cache, &mem192_mem_cache, &mem256_mem_cache, &mem384_mem_cache, &mem512_mem_cache
  };
  size_t block_sizes[12] = { 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512 };

  if(size < sizeof(long int))
    return kmem_cache_alloc(&mem8_mem_cache, 0);

  if(size > 512)
    RET_MSG(NULL, "Block sizes above 512 bytes aren't supported yet.");
  else {
    for(size_t i = 0; i < 12; i++) {
      if(i + 1 == 12 || (size >= block_sizes[i] && size < block_sizes[i+1]))
        return kmem_cache_alloc(caches[i], 0);
    }
  }

  kassert(false);
  RET_MSG(NULL, "Unable to allocate memory.");
}

void *kcalloc(size_t elem_size, size_t n_elems, size_t align)
{
  void *block = kmalloc(elem_size * n_elems, align);

  if(block != NULL)
    kmemset(block, 0, elem_size * n_elems);

  return block;
}

void *krealloc(void *ptr, size_t new_size, size_t align)
{
  struct kmem_cache *caches[12] = { &mem8_mem_cache, &mem16_mem_cache, &mem24_mem_cache,
           &mem32_mem_cache, &mem48_mem_cache, &mem64_mem_cache, &mem96_mem_cache,
           &mem128_mem_cache, &mem192_mem_cache, &mem256_mem_cache, &mem384_mem_cache, &mem512_mem_cache
  };

  for(size_t i = 0; i < 12; i++) {
    list_t *lists[2] = { &caches[i]->partially_used_slabs, &caches[i]->used_slabs };

    for(size_t j = 0; j < 2; j++) {
      for(list_node_t *node = lists[j]->head; node != NULL; node = node->next) {
        struct kslab_node *slab_node = (struct kslab_node *)GET_KSLAB_NODE(node);

        if((uintptr_t)ptr >= (uintptr_t)slab_node && (uintptr_t)ptr < (uintptr_t)slab_node + (1ul << slab_node->log2_order)) {
          void *new_ptr = kmalloc(new_size, align);

          if(new_ptr == NULL)
            RET_MSG(NULL, "Unable to allocate additional memory.");

          kmemcpy(new_ptr, ptr, caches[i]->size < new_size ? caches[i]->size : new_size);

          if(IS_ERROR(kmem_cache_free(caches[i], ptr)))
            RET_MSG(NULL, "Not a valid memory object.");
          return new_ptr;
        }
      }
    }
  }

  RET_MSG(NULL, "Unable to reallocate memory. Memory object was not found.");
}

void kfree(void *ptr)
{
  struct kmem_cache *caches[12] = { &mem8_mem_cache, &mem16_mem_cache, &mem24_mem_cache,
           &mem32_mem_cache, &mem48_mem_cache, &mem64_mem_cache, &mem96_mem_cache,
           &mem128_mem_cache, &mem192_mem_cache, &mem256_mem_cache, &mem384_mem_cache, &mem512_mem_cache
  };

  for(size_t i = 0; i < 12; i++) {
    list_t *lists[2] = { &caches[i]->partially_used_slabs, &caches[i]->used_slabs };

    for(size_t j = 0; j < 2; j++) {
      for(list_node_t *node = lists[j]->head; node != NULL; node = node->next) {
        struct kslab_node *slab_node = (struct kslab_node *)GET_KSLAB_NODE(node);

        if((uintptr_t)ptr >= (uintptr_t)slab_node && (uintptr_t)ptr < (uintptr_t)slab_node + (1ul << slab_node->log2_order)) {
          if(IS_ERROR(kmem_cache_free(caches[i], ptr)))
            kprintf("warning: %p does not point to a valid memory object.\n", ptr);
          return;
        }
      }
    }
  }

  kprintf("warning: %p does not point to a valid memory object.\n", ptr);
}
