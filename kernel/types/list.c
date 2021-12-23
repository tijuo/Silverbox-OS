#include <kernel/types/list.h>
#include <kernel/thread.h>
#include <kernel/error.h>
#include <kernel/debug.h>

CONST static int list_ptr_compare(void *p1, void *p2);

int list_ptr_compare(void *p1, void *p2) {
  return (intptr_t)p1 - (intptr_t)p2;
}

int list_insert_at_end(list_t *list, void *item, size_t item_size, bool at_tail) {
  list_node_t *node = (list_node_t *)kmalloc(sizeof(list_node_t));

  if(!node)
    return E_FAIL;

  node->item = kmalloc(item_size);

  if(!node->item)
    return E_FAIL;
  else
    memcpy(node->item, item, item_size);

  return list_insert_node_at_end(list, at_tail, node);
}

void list_insert_node_at_end(list_t *list, bool at_tail, list_node_t *new_node) {
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

int list_remove_from_end(list_t *list, void *item, size_t item_size, bool at_tail) {
  list_node_t *node;

  int retval = list_remove_node_from_end(list, at_tail, &node);

  if(IS_ERROR(retval))
    return E_FAIL;
  else {
    if(item)
      memcpy(item, node->item, item_size);

    kfree(node);

    return E_OK;
  }
}

int list_remove_node_from_end(list_t *list, bool at_tail, list_node_t **removed_node) {
  kassert(!!list->head == !!list->tail);
  list_node_t *node;

  if(is_list_empty(list))
    return E_FAIL;
  else {
    if(at_tail) {
      node = list->tail;

      list->tail = node->prev;

      if(list->tail)
        list->tail->next = NULL;
      else
        list->head = NULL;
    }
    else {
      node = list->head;

      if(item)
        *item = node->item;

      list->head = node->next;
      kfree(node);

      if(list->head)
        list->head->prev = NULL;
      else
        list->tail = NULL;
    }

    if(removed_node)
      *removed_node = node;

    return E_OK;
  }
}

int list_remove_item(list_t *list, void *item, int (*compare)(void *, void *)) {
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
      return E_OK;
    }
  }

  return E_NOT_FOUND;
}

int list_remove(list_t *list, void *item) {
  return list_remove_deep(list, item, list_ptr_compare);
}
