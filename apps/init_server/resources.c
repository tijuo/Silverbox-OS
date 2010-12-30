#include "phys_alloc.h"
#include "resources.h"

static int _attach_op(void *key, size_t keysize, struct ResourcePool *pool,
    SBAssocArray *assocArray);
static struct ResourcePool *_detach_op(void *key, size_t keysize, 
    SBAssocArray *assocArray);
static struct ResourcePool *_lookup_op(void *key, size_t keysize, 
    SBAssocArray *assocArray);

static int _attach_op(void *key, size_t keysize, struct ResourcePool *pool,
    SBAssocArray *assocArray)
{
  if( !pool || !key )
    return -1;

  if( sbAssocArrayInsert(assocArray, key, keysize, pool, sizeof pool) != 0 )
    return -1;
  else
    return 0;
}

static struct ResourcePool *_detach_op(void *key, size_t keysize, 
    SBAssocArray *assocArray)
{
  struct ResourcePool *pool = NULL;

  if( sbAssocArrayRemove(assocArray, key, keysize, (void **)&pool, NULL) != 0 )
    return NULL;
  else
    return pool;
}

static struct ResourcePool *_lookup_op(void *key, size_t keysize, 
    SBAssocArray *assocArray)
{
  struct ResourcePool *pool = NULL;

  if( sbAssocArrayLookup(assocArray, key, keysize, (void **)&pool, NULL) != 0 )
    return NULL;
  else
    return pool;
}

int __create_resource_pool(struct ResourcePool *pool, void *phys_aspace)
{
  static rspid_t counter=2;

  if( !pool || IS_PNULL(phys_aspace) )
    return -1;  

  if( sbArrayCreate(&pool->tids) < 0 )
    return -1;

  pool->ioBitmaps.phys1 = alloc_phys_page(NORMAL, page_dir);
  pool->ioBitmaps.phys2 = alloc_phys_page(NORMAL, page_dir);

  if( IS_PNULL(pool->ioBitmaps.phys1) || IS_PNULL(pool->ioBitmaps.phys2) )
  {
    if( !IS_PNULL(pool->ioBitmaps.phys1) )
      free_phys_page(pool->ioBitmaps.phys1);

    if( !IS_PNULL(pool->ioBitmaps.phys2) )
      free_phys_page(pool->ioBitmaps.phys2);

    sbArrayDelete(&pool->tids);

    return -1;
  }

  init_addr_space(&pool->addrSpace, phys_aspace);
  attach_phys_aspace(pool, phys_aspace);

  pool->execInfo = NULL;

  setPage(pool->ioBitmaps.phys1, 0xFF);
  setPage(pool->ioBitmaps.phys2, 0xFF);

  pool->id = counter++;

  return 0;
}

struct ResourcePool *_create_resource_pool(void *phys_aspace)
{
  struct ResourcePool *pool = malloc(sizeof(struct ResourcePool));
  
  if( __create_resource_pool(pool, phys_aspace) == 0 )
    return pool;

  free(pool);
  return NULL;
}

struct ResourcePool *create_resource_pool(void)
{
  void *phys = alloc_phys_page(NORMAL, page_dir);

  clearPage(phys);
  return _create_resource_pool(phys);
}

int destroy_resource_pool(struct ResourcePool *pool)
{
  if( !pool )
    return -1;

  sbArrayDelete(&pool->tids);
  detach_phys_aspace(pool->addrSpace.phys_addr);
  delete_addr_space(&pool->addrSpace);

  free_phys_page(pool->addrSpace.phys_addr);
  free_phys_page(pool->ioBitmaps.phys1);
  free_phys_page(pool->ioBitmaps.phys2);

  // XXX: release execInfo

  free(pool);

  return 0;
}

int attach_resource_pool(struct ResourcePool *pool)
{
  return _attach_op( !pool ? NULL : &pool->id, !pool ? 0 : sizeof pool->id,
    pool, &resourcePools );
}

struct ResourcePool *detach_resource_pool(rspid_t id)
{
  return _detach_op( &id, sizeof id, &resourcePools );
}

struct ResourcePool *lookup_rspid(rspid_t id)
{
  return _lookup_op(&id, sizeof id, &resourcePools);
}

int attach_tid(struct ResourcePool *pool, tid_t tid)
{
  if( tid == NULL_TID )
    return -1;
  else
    return _attach_op( &tid, sizeof tid, pool, &tidTable );
}

struct ResourcePool *detach_tid(tid_t tid)
{
  if( tid == NULL_TID )
    return NULL;
  else
    return _detach_op(&tid, sizeof tid, &tidTable);
}

struct ResourcePool *lookup_tid(tid_t tid)
{
  if( tid == NULL_TID )
    return NULL;
  else
    return _lookup_op(&tid, sizeof tid, &tidTable);
}

int attach_phys_aspace(struct ResourcePool *pool, void *addr)
{
  if( IS_PNULL(addr) )
    return -1;
  else
    return _attach_op(&addr, sizeof addr, pool, &physAspaceTable);
}

struct ResourcePool *detach_phys_aspace(void *addr)
{
  if( IS_PNULL(addr) )
    return NULL;
  else
    return _detach_op(&addr, sizeof addr, &physAspaceTable);
}

struct ResourcePool *lookup_phys_aspace(void *addr)
{
  if( IS_PNULL(addr) )
    return NULL;
  else
    return _lookup_op(&addr, sizeof addr, &physAspaceTable);
}

