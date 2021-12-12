#include <kernel/types/list.h>
#include <kernel/thread.h>
#include <kernel/error.h>
#include <kernel/debug.h>

/**
 *  Insert an item into a list at the head or tail.
 *
 *  @param list The list onto which to push the item. (Must not be NULL)
 *  @param item A pointer to the item that is to be inserted.
 *  @param at_tail true, if the item should be added at the tail of the list.
 *         false, if it should be added at the head.
 */

void list_insert_at_end(list_t *list, void *item, bool at_tail) {
  list_node_t *new_node = kmalloc(sizeof(list_node_t));

  new_node->item = item;

  if(is_list_empty(list)) {
    list->head = new_node;
    list->tail = new_node;
    new_node->next = NULL;
    new_node->prev = NULL;
  }
  else if(at_tail) {
    new_node->next = NULL;
    list->tail->next = new_node;
    new_node->prev = list->tail;
    list->tail = new_node;
  }
  else {
    new_node->prev = NULL;
    list->head->prev = new_node;
    new_node->next = list->head;
    list->head = new_node;
  }
}

/**
 *  Remove an item from either the head or tail of a list.
 *
 *  @param list The list from which to remove the item.
 *  @param item A pointer to the removed item. If NULL, the removed item
           will not be returned.
 *  @param at_tail true, if the thread should be removed from the tail of the list. false, if it
 *         should be removed from the head.
 *  @return E_OK, if the list successfully removed the item. E_FAIL, if empty.
 */

int list_remove_from_end(list_t *list, void **item, int at_tail) {
  assert(!!list->head == !!list->tail)

  if(is_list_empty(list))
    return E_FAIL;
  else {
    if(at_tail) {
      list_node_t *node = list->tail;

      if(item) {
        *item = node->item;

      list->tail = node->prev;
      kfree(node);

      if(list->tail)
        list->tail->next = NULL;
      else
        list->head = NULL;
    }
    else {
      list_node_t *node = list->head;

      if(item) {
        *item = node->item;

      list->head = node->next;
      kfree(node);

      if(list->head)
        list->head->prev = NULL;
      else
        list->tail = NULL;
    }

    return E_OK;
  }
}

/**
 *  Removes the first occurrence of an item from a list.
 *
 *  @param list The list from which the thread will be removed. (Must not be null)
 *  @param item The item to be removed.
 *  @param compare A comparison function pointer that determines whether two elements
 *  are equivalent. Returns 0 if equivalent, non-zero, otherwise.
 *  @return E_OK, on successful removal. E_NOT_FOUND, if the item wasn't in the list
 */

int list_remove(list_t *list, void *item, int (*compare)(void *, void *)) {
  for(list_node_t *node=list->head; node; node = node->next) {
    if(compare(node->item, item) == 0) {
      if(node->prev) {
        if(node->next)
          node->prev->next = node->next->prev;
        else {
          node->prev->next = NULL;
          list->tail = node->prev;
        }
      }
      else {
        if(node->next) {
          node->next->prev = NULL;
          list->head = node->next;
        }
        else {
          list->head = NULL;
          list->tail = NULL;
        }
      }

      kfree(node);
      return E_OK
    }
  }

  return E_NOT_FOUND;
}
