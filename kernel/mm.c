#include <kernel/mm.h>
#include <kernel/buddy.h>

list_t free_block_list;
//list_t dma_free_slab_list;

struct buddy_allocator phys_allocator;

/** The area of memory reserved for the physical memory allocator.
    In this case, the free block arrays for the buddy allocator. */
uint8_t free_phys_blocks[0x40000];

void *kalloc_page(void) {
  list_node_t *node = list_peek_tail(&free_slab_list);

  assert((uintptr_t)node->item - (uintptr_t) >= PAGE_SIZE);
  assert(node->item != NULL);

  if((uintptr_t)node->item - (uintptr_t)node <= PAGE_SIZE) {
    if(IS_ERROR(list_remove_node_from_end(&free_slab_list, true, NULL)))
      RET_MSG(NULL, "Unable to remove node from free list.");
  }
  else {
    list_node_t *new_node = (list_node_t *)((uintptr_t)node + PAGE_SIZE);

    new_node->item = node->item;

    if(IS_ERROR(list_replace_node(&free_slab_list, new_node, NULL)))
      RET_MSG(NULL, "Unable to replace node in free list.");
  }

  return node;
}

void kfree_page(void *addr) {
  assert((uintptr_t)addr & (PAGE_SIZE - 1) == 0);

  list_node_t *node = (void *)addr;

  node->item = (void *)((uintptr_t)node + PAGE_SIZE);

  if(IS_ERROR(list_insert_node_at_end(&free_slab_list, true, node)))
    RET_MSG(, "Unable to insert free node in free list.");
}

int kslab_init(void *start, size_t slab_size) {
  struct slab_node *slab_node = (struct slab_node *)((uintptr_t)start + slab_size - sizeof (struct slab_node));

  if(slab_size == 0)
    RET_MSG(E_FAIL, "Slab size cannot be zero.");
  else if(slab_size > UINT16_MAX+1)
    RET_MSG(E_FAIL, "Slab sizes over 65536 bytes is not supported.");

  kmemset(slab_node, 0, sizeof *slab_node);

  slab_node->log2_order = __bsrq(slab_size);

  if(__bsfq(slab_size) != node->log2_order)
    node->log2_order++;

  slab_node->node->item = start;

  return E_OK;
}

int kmem_cache_init(struct kmem_cache *mem_cache, int obj_type, size_t obj_size, size_t align,
  kmem_cache_ctor_t ctor, kmem_cache_dtor_t dtor)
{
  if(obj_size < sizeof(uint64_t))
    obj_size = sizeof(uint64_t);

  if(obj_size > PAGE_SIZE)
    RET_MSG(E_FAIL, "Memory objects larger than 4096 bytes is not supported.");

  if(align < sizeof(uint64_t))
    align = sizeof(uint64_t);

  mem_cache->obj_type = obj_type;
  mem_cache->size = obj_size;
  mem_cache->align = align;
  mem_cache->constructor = ctor;
  mem_cache->destructor = dtor;

  list_init(&mem_cache->free_slabs);
  list_init(&mem_cache->partially_used_slabs);
  list_init(&mem_cache->used_slabs);

  return E_OK;
}

int kmem_cache_init_free_slab(struct kmem_cache *mem_cache, struct slab_node *slab_node) {
  const list_t * const lists[3] = { &mem_cache->free_slabs, &mem_cache->partially_used_slabs, &mem_cache->used_slabs };

  for(size_t i=0; i < 3; i++) {
    if(!list_is_empty(lists[i])) {
      slab_node->color = (uint8_t)((((struct slab_node *)list_peek_head(lists[i])->item)->color + 1)
        % mem_cache_num_colors(mem_cache, slab_node));
      break;
    }
  }

  if(mem_cache->size < PAGE_SIZE / 8) {
    size_t buffer_count = (((1u << slab_node->log2_order) - sizeof *slab_node - mem_cache->align * slab_node->color)
      / (mem_cache->size + sizeof(void *));

    for(size_t i=0; i < buffer_count; i++) {
      void *buf = (void *)((uintptr_t)slab_node->node->item
        + sizeof(uint64_t) * slab_node->color + i*(mem_cache->size + sizeof(void *)));
      void **next = (void **)((uintptr_t)buf + mem_cache->size);

      if(mem_cache->constructor)
        mem_cache->constructor(buf, mem_cache->size);

      *next = i + 1 >= buffer_count ? NULL : (void *)((uintptr_t)next + sizeof *next);
    }
  }
  // TODO: implement large mem_cache objects
  else
    RET_MSG(E_FAIL, "Memory objects greater than or equal to 512 bytes is not supported (yet).");

  return E_OK;
}

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
