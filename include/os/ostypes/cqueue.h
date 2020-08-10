#ifndef CQUEUE_H
#define CQUEUE_H

#include <stddef.h>

#define CQUEUE_NUM_FREE( cqueue )	({ __typeof__ (cqueue) q=(cqueue); \
                                         ((q->n_elems - CQUEUE_NUM_FILL(q)); })

#define CQUEUE_NUM_FILL( cqueue )	({ __typeof__ (cqueue) q=(cqueue); \
                                          (q->tail >= q->head ? \
                                        q->tail - q->head : \
                                        q->tail + q->n_elems - q->head); })
#define CQUEUE_IS_EMPTY( cqueue )	((cqueue)->state == CQUEUE_EMPTY)
#define CQUEUE_IS_FULL( cqueue )        ((cqueue)->state == CQUEUE_FULL)

enum CQueueState { CQUEUE_PARTIAL, CQUEUE_EMPTY, CQUEUE_FULL };

struct CQueue
{
  void *queue;
  size_t elem_size, n_elems;
  unsigned head, tail;
  enum CQueueState state;
};

int _cqueue_init( struct CQueue *cqueue, void *queue, size_t elem_size,
                  size_t n_elems );
struct CQueue *cqueue_init( size_t elem_size, size_t n_elems  );
int cqueue_put( struct CQueue *cqueue, void *buffer, unsigned num_elems );
int cqueue_get( struct CQueue *cqueue, void *buffer, unsigned num_elems );
int cqueue_peek( struct CQueue *cqueue, void *buffer, unsigned index,
                 size_t n_elems );
int cqueue_poke( struct CQueue *cqueue, void *buffer, unsigned index,
                 size_t n_elems );
#endif /* CQUEUE_H */
