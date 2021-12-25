#ifndef KERNEL_TYPES_LIST_H
#define KERNEL_TYPES_LIST_H

#include <util.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

union list_item {
  void *item_void_ptr;
  uint64_t item_uint64_t;
  uint32_t item_uint32_t;
  uint16_t item_uint16_t;
  uint8_t item_uint8_t;
  int64_t item_int64_t;
  int32_t item_int32_t;
  int16_t item_int16_t;
  int8_t item_int8_t;
};

typedef struct list_node {
  union list_item item;
  struct list_node *prev;
  struct list_node *next;
} list_node_t;

typedef struct {
  list_node_t *head;
  list_node_t *tail;
} list_t;

#define DECL_LIST_INSERT_AT_END(typename) \
NON_NULL_PARAMS int list_insert_##typename##_at_end(list_t *list, typename item, UNUSED size_t item_size, bool at_tail)

#define LIST_INSERT_AT_END(typename) \
DECL_LIST_INSERT_AT_END(typename) { \
  list_node *node = (list_node_t *)kmalloc(sizeof(list_node_t)); \
\
  if(!node) \
    return E_FAIL; \
\
  node->item.item_##typename = item; \
  return list_insert_node_at_end(list, at_tail, node); \
}

#define DECL_LIST_REMOVE_FROM_END(typename) \
NON_NULL_PARAM(1) int list_remove_##typename##_from_end(list_t *list, typename *item, UNUSED size_t item_size, bool at_tail)

#define LIST_REMOVE_FROM_END(typename) \
DECL_LIST_REMOVE_FROM_END(typename) { \
  list_node_t *node; \
\
  int retval = list_remove_node_from_end(list, at_tail, &node); \
\
  if(IS_ERROR(retval)) \
    return E_FAIL; \
  else { \
    if(item) \
      *item = node->item.item_##typename; \
\
    kfree(node); \
\
    return E_OK; \
  } \
}

DECL_LIST_INSERT_AT_END(int8_t);
DECL_LIST_INSERT_AT_END(int16_t);
DECL_LIST_INSERT_AT_END(int32_t);
DECL_LIST_INSERT_AT_END(int64_t);
DECL_LIST_INSERT_AT_END(uint8_t);
DECL_LIST_INSERT_AT_END(uint16_t);
DECL_LIST_INSERT_AT_END(uint32_t);
DECL_LIST_INSERT_AT_END(uint64_t);

DECL_LIST_REMOVE_FROM_END(int8_t);
DECL_LIST_REMOVE_FROM_END(int16_t);
DECL_LIST_REMOVE_FROM_END(int32_t);
DECL_LIST_REMOVE_FROM_END(int64_t);
DECL_LIST_REMOVE_FROM_END(uint8_t);
DECL_LIST_REMOVE_FROM_END(uint16_t);
DECL_LIST_REMOVE_FROM_END(uint32_t);
DECL_LIST_REMOVE_FROM_END(uint64_t);

#define list_insert_at_end(list, item, at_tail) _Generic((item), \
  int8_t: list_insert_int8_t_at_end, \
  int16_t: list_insert_int16_t_at_end, \
  int32_t: list_insert_int32_t_at_end, \
  int64_t: list_insert_int64_t_at_end, \
  uint8_t: list_insert_uint8_t_at_end, \
  uint16_t: list_insert_uint16_t_at_end, \
  uint32_t: list_insert_uint32_t_at_end, \
  uint64_t: list_insert_uint64_t_at_end, \
  default: list_insert_item_at_end)((list), (item), sizeof((item)), (at_tail))

#define list_remove_from_end(list, item, at_tail) _Generic((item), \
  int8_t: list_remove_int8_t_from_end, \
  int16_t: list_remove_int16_t_from_end, \
  int32_t: list_remove_int32_t_from_end, \
  int64_t: list_remove_int64_t_from_end, \
  uint8_t: list_remove_uint8_t_from_end, \
  uint16_t: list_remove_uint16_t_from_end, \
  uint32_t: list_remove_uint32_t_from_end, \
  uint64_t: list_remove_uint64_t_from_end, \
  default: list_remove_item_from_end)((list), (item), sizeof((item)), (at_tail))

/**
  Initializes a linked list.

  @param list The linked list.
*/

NON_NULL_PARAMS static inline void list_init(list_t *list) {
  list->head = NULL;
  list->tail = NULL;
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

NON_NULL_PARAM(1) int list_insert_item_at_end(list_t *l, void *item, size_t item_size, bool at_tail);

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

NON_NULL_PARAM(1) int list_remove_item_from_end(list_t *l, void *item, size_t item_size, bool at_tail);

/**
 *  Removes the first occurrence of an item from a list.
 *
 *  @param list The list from which the thread will be removed.
 *  (Must not be null)
 *  @param item The pointer to the item to be removed.
 *  @return E_OK, on successful removal. E_NOT_FOUND, if the item wasn't
 *  in the list
 */

NON_NULL_PARAM(1) int list_remove_ptr(list_t *l, void *item);

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

/**
  Insert a node into a list after another node.

  @param list The linked list.
  @param node The node that will be inserted into the list.
  @param target The inserted node will be placed after this node. It is assumed that
  target is already in the linked list.
*/

NON_NULL_PARAMS void list_insert_node_after(list_t *list, list_node_t *node, list_node_t *target);

/**
  Insert a node into a list before another node.

  @param list The linked list.
  @param node The node that will be inserted into the list.
  @param target The inserted node will be placed before this node. It is assumed that
  target is already in the linked list.
*/

NON_NULL_PARAMS void list_insert_node_before(list_t *list, list_node_t *node, list_node_t *target);

/**
  Detach a node from a linked list.

  @param list The linked list.
  @param node The node to detached. It is assumed that the node is already in the linked list.
*/

NON_NULL_PARAMS void list_unlink_node(list_t *list, list_node_t *node);

NON_NULL_PARAM(1) int list_remove(list_t *list, void *item);

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
