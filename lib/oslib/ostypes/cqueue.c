#include <os/ostypes/cqueue.h>
#include <stdlib.h>
#include <string.h>

int _cqueue_init( struct CQueue *cqueue, void *queue, size_t elem_size, size_t n_elems )
{
  if( cqueue == NULL || queue == NULL )
    return -1;

  cqueue->queue = queue;
  cqueue->head = cqueue->tail = 0;
  cqueue->state = CQUEUE_EMPTY;
  cqueue->elem_size = elem_size;
  cqueue->n_elems = n_elems;

  return 0;
}

struct CQueue *cqueue_init( size_t elem_size, size_t n_elems  )
{
  struct CQueue *cqueue;
  void *queue;

  if( elem_size == 0 || n_elems == 0 )
    return NULL;

  cqueue = malloc( sizeof(struct CQueue) );
  
  if( cqueue == NULL )
    return NULL;

  queue = malloc( elem_size * n_elems );

  if( queue == NULL )
  {
    free( cqueue );
    return NULL;
  }

  if( _cqueue_init(cqueue, queue, elem_size, n_elems ) != 0 )
    return NULL;
  else
    return cqueue;
}

// Returns 0 when queue is not full, 1 when queue is full

int cqueue_put( struct CQueue *cqueue, void *buffer, unsigned num_elems )
{
  unsigned char *buf = (unsigned char *)buffer;

  if( buffer == NULL || cqueue == NULL )
    return -1;

  while( num_elems-- )
  {
    if( cqueue->state == CQUEUE_FULL )
      return -2;

    memcpy((void *)((unsigned)cqueue->queue+cqueue->tail*cqueue->elem_size), 
           buf, cqueue->elem_size );
    buf += cqueue->elem_size;
    cqueue->tail = (cqueue->tail + 1) % cqueue->n_elems;
    cqueue->state = (cqueue->tail == cqueue->head ? CQUEUE_FULL : 
                    CQUEUE_PARTIAL );
  }

  if( cqueue->state == CQUEUE_FULL )
    return 1;
  else
    return 0;
}

// Returns 0 when queue is not empty
// Returns 1 when queue is empty

int cqueue_get( struct CQueue *cqueue, void *buffer, unsigned num_elems )
{
  unsigned char *buf = (unsigned char *)buffer;

  if( cqueue == NULL )
    return -1;

  while( num_elems-- )
  {
    if( cqueue->state == CQUEUE_EMPTY )
      return -2;

    if( buffer != NULL )
    {
      memcpy(buf, (void *)((unsigned)cqueue->queue + cqueue->head * 
             cqueue->elem_size), cqueue->elem_size );
      buf += cqueue->elem_size;
    }
    cqueue->head = (cqueue->head + 1) % cqueue->n_elems;
    cqueue->state = (cqueue->tail == cqueue->head ? CQUEUE_EMPTY : 
                    CQUEUE_PARTIAL );
  }

  if( cqueue->state == CQUEUE_EMPTY )
    return 1;
  else
    return 0;
}

int cqueue_peek( struct CQueue *cqueue, void *buffer, unsigned index, 
                 size_t n_elems )
{
  unsigned char *buf = (unsigned char *)buffer;

  if( cqueue == NULL || buffer == NULL  )
    return -1;

  if( index >= cqueue->n_elems )
    index %= cqueue->n_elems;

  while( n_elems-- )
  {
    memcpy(buf, (void *)((unsigned)cqueue->queue + index * 
           cqueue->elem_size), cqueue->elem_size);
    buf += cqueue->elem_size;
    index = (index + 1) % cqueue->n_elems;
  }
  return 0;
}

int cqueue_poke( struct CQueue *cqueue, void *buffer, unsigned index,
                 size_t n_elems )
{
  unsigned char *buf = (unsigned char *)buffer;

  if( cqueue == NULL || buffer == NULL || index >= cqueue->n_elems )
    return -1;

  while( n_elems-- )
  {
    memcpy((void *)((unsigned)cqueue->queue + index *
           cqueue->elem_size), buf, cqueue->elem_size);
    buf += cqueue->elem_size;
    index = (index + 1) % cqueue->n_elems;
  }
  return 0;
}

