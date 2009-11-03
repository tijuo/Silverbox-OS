#ifndef _QUEUE_H
#define _QUEUE_H

#include "list.h"

void queue_init(struct ListType *queue, void *(*malloc_func)(size_t), \
    void (*free_func)(void *));
int queue_enqueue( int key, void **element_ptr, struct ListType *queue );
int queue_dequeue( int *key, void **element_ptr, struct ListType *queue );

#endif /* _QUEUE_H */
