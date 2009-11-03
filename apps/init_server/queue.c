#include "list.h"

void queue_init(struct ListType *queue, void *(*malloc_func)(size_t), void (*free_func)(void *))
{
  list_init(queue, malloc_func, free_func);
}

int queue_enqueue( int key, void **element_ptr, struct ListType *queue )
{
  struct ListNode *new_node;

  if( queue == NULL )
    return -1;

  new_node = queue->malloc( sizeof(struct ListNode) );

  if( new_node == NULL )
    return -1;

  new_node->key = key;
  new_node->element = element_ptr;
  new_node->prev = queue->head;
  new_node->next = NULL;

  if( queue->tail != NULL )
  {
    queue->tail->next = new_node;

    if( queue->head == queue->tail )
      queue->head = new_node;
  }

  queue->tail = new_node;
  
  return 0;
}

int queue_dequeue( int *key, void **element_ptr, struct ListType *queue )
{
  struct ListNode *node;
  
  if( queue == NULL )
    return -1;

  if( queue->head == NULL )
    return 1;
  
  if( key != NULL )
    key = queue->head->key;

  if( element_ptr != NULL )
    *(unsigned int **)element_ptr = *(unsigned int **)queue->head->element;
  
  node = queue->head;
  
  if( node->next != NULL )
    queue->head = node->next;
  else /* This should imply that queue->head == queue->tail */
    queue->tail = queue->head = NULL;
  
  node->free( node );

  return 0;
}
