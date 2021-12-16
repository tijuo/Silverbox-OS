#ifndef KERNEL_TYPES_LIST_H
#define KERNEL_TYPES_LIST_H

#include <util.h>
#include <stdbool.h>

typedef struct list_node {
  void *item;
  struct list_node *prev;
  struct list_node *next;
} list_node_t;

typedef struct {
  list_node_t *head;
  list_node_t *tail;
} list_t;

NON_NULL_PARAM(1) void list_insert_at_end(list_t *l, void *item, bool at_tail);
NON_NULL_PARAM(1) int list_remove_from_end(list_t *l, void **item, bool at_tail);
NON_NULL_PARAMS int list_remove(list_t *l, void *item);
NON_NULL_PARAMS int list_remove_deep(list_t *list, void *item, int (*compare)(void *, void *));

/// Inserts an item at the head of a list.
#define list_enqueue(list, item)   list_insert_at_end(list, item, false)

/// Removes an item from the tail of a list
#define list_dequeue(list, item)   list_remove_from_end(list, item, true)

/// Pushes an item to the head of the list
#define list_push(list, item)      list_insert_at_end(list, item, false)

/// Pops an item from the head of the list
#define list_pop(list, item)       list_remove_from_end(list, item, false)

NON_NULL_PARAMS static inline bool is_list_empty(list_t *l) {
  return !l->head;
}

#endif /* KERNEL_TYPES_LIST_H */
