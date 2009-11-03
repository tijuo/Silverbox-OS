#include "list.h"

void list_init(struct ListType *list, void *(*malloc_func)(size_t), void (*free_func)(void *))
{
  if( list == NULL )
    return;

  list->head = NULL;
  list->tail = NULL;
  list->malloc = malloc_func;
  list->free = free_func;
}

int list_insert(int key, void *element, struct ListType *list)
{
  struct ListNode *node;

  if( list == NULL )
    return -1;

  node = list->malloc(sizeof(struct ListNode));

  if( node == NULL )
    return -1;

  node->prev = node->next = NULL;
  node->element = element; 
  node->key = key;

  if( list->head == NULL || list->tail == NULL )
  {
    list->head = node;
    list->tail = node;
  }
  else
  {
    list->tail->next = node;
    node->prev = list->tail;
    list->tail = node;
  }
  return 0;
}

int list_remove(int key, struct ListType *list)
{
  struct ListNode *node;

  if( list == NULL )
    return -1;

  for( node=list->head; node != NULL; node = node->next )
  {
    if( node->key == key )
      break;
  }

  if( node == NULL )
    return -1;

  if( node->prev != NULL )
    node->prev->next = node->next;

  if( node->next != NULL )
    node->next->prev = node->prev;

  if( list->head == node )
    list->head = node->next;

  if( list->tail == node )
    list->tail = node->prev;

  list->free(node);

  return 0;
}

int list_get_element(int key, void **element_ptr, struct ListType *list)
{
  struct ListNode *node;

  if( list == NULL )
    return -1;

  for( node=list->head; node != NULL; node = node->next )
  {
    if( node->key == key )
      break;
  }

  if( node == NULL )
    return -1;
  else if( element_ptr != NULL )
    *(unsigned int **)element_ptr = (unsigned int *)node->element;

  return 0;
}

bool list_lookup(int key, struct ListType *list)
{
  if( list == NULL )
    return -1;

  return (list_get_element(key, (void **)NULL, list) == 0) ? true : false;
}
