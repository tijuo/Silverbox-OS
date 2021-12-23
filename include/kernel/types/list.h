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

NON_NULL_PARAMS static inline void list_init(list_t *list) {
  list->head = NULL;
  list->tail = NULL;

  return E_OK;
}

/**
 *  Insert a linked list node into a list at the head or tail.
 *
 *  @param list The list onto which to push the node. (Must not be NULL)
 *  @param at_tail true, if the node should be added at the tail of the list.
 *         false, if it should be added at the head.
 *  @param node The linked list node to be inserted. The node's item should be set
 *  by the caller.
 */

NON_NULL_PARAM(1) void list_insert_node_at_end(list_t *list, bool at_tail, list_node_t *new_node);

/**
 *  Remove a linked list node from either the head or tail of a list.
 *
 *  @param list The list from which to remove the item.
 *  @param at_tail true, if the thread should be removed from the tail of
 *  the list. false, if it should be removed from the head.
 *  @param item A pointer to the removed node pointer. If NULL, the removed node
 *  will not be returned.
 *  @return E_OK, if the list successfully removed the node. E_FAIL, if the
 *  list is empty.
 */

NON_NULL_PARAM(1) int list_remove_node_from_end(list_t *list, bool at_tail, list_node_t **removed_node);

/**
 *  Insert an item into a list at the head or tail.
 *
 *  @param list The list onto which to push the item. (Must not be NULL)
 *  @param item A pointer to the item that is to be inserted.
 *  @param item_size The size of the item in bytes.
 *  @param at_tail true, if the item should be added at the tail of the list.
 *         false, if it should be added at the head.
 */

NON_NULL_PARAM(1) void list_insert_at_end(list_t *l, void *item, size_t item_size, bool at_tail);

/**
 *  Remove an item from either the head or tail of a list.
 *
 *  @param list The list from which to remove the item.
 *  @param item A pointer to the removed item. If NULL, the removed item
 *  will not be returned.
 *  @param item_size The size of the item in bytes.
 *  @param at_tail true, if the thread should be removed from the tail of
 *  the list. false, if it should be removed from the head.
 *  @return E_OK, if the list successfully removed the item. E_FAIL, if
 *  the list is empty.
 */

NON_NULL_PARAM(1) int list_remove_from_end(list_t *l, void *item, size_t item_size, bool at_tail);

/**
 *  Removes the first occurrence of an item from a list.
 *
 *  @param list The list from which the thread will be removed.
 *  (Must not be null)
 *  @param item The pointer to the item to be removed.
 *  @return E_OK, on successful removal. E_NOT_FOUND, if the item wasn't
 *  in the list
 */

NON_NULL_PARAM(1) int list_remove(list_t *l, void *item);

/**
 *  Removes the first occurrence of an item from a list.
 *
 *  @param list The list from which the thread will be removed.
 *  (Must not be null)
 *  @param item The item to be removed.
 *  @param compare A comparison function pointer that determines whether
 *  two elements
 *  are equivalent. Returns 0 if equivalent, non-zero, otherwise.
 *  @return E_OK, on successful removal. E_NOT_FOUND, if the item wasn't
 *  in the list
 */

NON_NULL_PARAM(1) int list_remove_item(list_t *list, void *item, int (*compare)(void *, void *));

/// Inserts an item at the head of a list.
#define list_enqueue(list, item)   list_insert_at_end(list, item, false)

/// Removes an item from the tail of a list
#define list_dequeue(list, item)   list_remove_from_end(list, item, true)

/// Pushes an item to the head of the list
#define list_push(list, item)      list_insert_at_end(list, item, false)

/// Pops an item from the head of the list
#define list_pop(list, item)       list_remove_from_end(list, item, false)

/**
  Does the list contain any items?

  @param list The list
  @return true, if the list doesn't contain any items. false, otherwise.
*/

#define is_list_empty(list)     (!(list)->head)

#define list_peek_head(list)    ((list)->head)
#define list_peek_tail(list)    ((list)->tail)

#endif /* KERNEL_TYPES_LIST_H */
