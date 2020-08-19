#include <kernel/list.h>
#include <kernel/error.h>
#include <kernel/debug.h>
#include <kernel/kmalloc.h>

static list_node_t *_createListNode(int key, void *elem);

list_node_t *_createListNode(int key, void *elem)
{
  list_node_t *node = kmalloc(sizeof(list_node_t));

  if(!node)
    return NULL;

  node->key = key;
  node->elem = elem;
  node->next = node->prev = NULL;

  return node;
}

int listInit(list_t *list)
{
  list->head = list->tail = NULL;
  return E_OK;
}

int listDestroy(list_t *list)
{
  list_node_t *next;

  for(list_node_t *ptr=list->head; ptr != NULL; ptr=next)
  {
    next = ptr->next;
    kfree(ptr, sizeof *ptr);
  }

  list->head = list->tail = NULL;
  return E_OK;
}

int listInsertHead(list_t *list, int key, void *element)
{
  list_node_t *node = _createListNode(key, element);

  if(node == NULL)
    return E_FAIL;

  assert(element != NULL);

  if(list->head != NULL && list->tail != NULL)
  {
    list->head->prev = node;
    node->next = list->head;
  }

  list->head = node;

  if(!list->tail)
    list->tail = node;

  return E_OK;
}

int listInsertTail(list_t *list, int key, void *element)
{
  list_node_t *node = _createListNode(key, element);

  if(node == NULL)
    return E_FAIL;

  assert(element != NULL);

  if(list->head != NULL && list->tail != NULL)
  {
    list->tail->next = node;
    node->prev = list->tail;
  }

  list->tail = node;

  if(!list->head)
    list->head = node;

  return E_OK;
}

int listFindFirst(list_t *list, int key, void **elemPtr)
{
  for(list_node_t *node=list->head; node != NULL; node = node->next)
  {
    if(node->key == key)
    {
      if(elemPtr != NULL)
        *elemPtr = node->elem;

      return E_OK;
    }
  }

  return E_FAIL;
}

int listFindLast(list_t *list, int key, void **elemPtr)
{
  for(list_node_t *node=list->tail; node != NULL; node = node->prev)
  {
    if(node->key == key)
    {
      if(elemPtr != NULL)
        *elemPtr = node->elem;

      return E_OK;
    }
  }

  return E_FAIL;
}

int listRemoveHead(list_t *list, void **elemPtr)
{
  list_node_t *prevHead = list->head;

  if(prevHead)
  {
    list->head = prevHead->next;

    if(list->head)
      list->head->prev = NULL;
    else
      list->tail = NULL;

    if(elemPtr)
      *elemPtr = prevHead->elem;

    kfree(prevHead, sizeof *prevHead);
    return E_OK;
  }
  else
    return E_FAIL;
}

int listRemoveTail(list_t *list, void **elemPtr)
{
  list_node_t *prevTail = list->tail;

  if(prevTail)
  {
    list->tail = prevTail->prev;

    if(list->tail)
      list->tail->next = NULL;
    else
      list->head = NULL;

    if(elemPtr)
      *elemPtr = prevTail->elem;

    kfree(prevTail, sizeof *prevTail);
    return E_OK;
  }
  else
    return E_FAIL;
}

int listRemoveFirst(list_t *list, int key, void **elemPtr)
{
  for(list_node_t *node=list->head; node != NULL; node = node->next)
  {
    if(node->key == key)
    {
      if(node->next != NULL)
        node->next->prev = node->prev;

      if(node->prev != NULL)
        node->prev->next = node->next;

      if(list->head == node || list->tail == node)
        list->head = list->tail = NULL;

      if(elemPtr)
        *elemPtr = node->elem;

      kfree(node, sizeof *node);
      return E_OK;
    }
  }

  return E_FAIL;
}

int listRemoveLast(list_t *list, int key, void **elemPtr)
{
  for(list_node_t *node=list->tail; node != NULL; node=node->prev)
  {
    if(node->key == key)
    {
      if(node->next != NULL)
        node->next->prev = node->prev;

      if(node->prev != NULL)
        node->prev->next = node->next;

      if(list->head == node || list->tail == node)
        list->head = list->tail = NULL;

      if(elemPtr)
        *elemPtr = node->elem;

      kfree(node, sizeof *node);
      return E_OK;
    }
  }

  return E_FAIL;
}

int listRemoveAll(list_t *list, int key)
{
  list_node_t *next;

  for(list_node_t *node=list->head; node != NULL; node=next)
  {
    next = node->next;

    if(node->key == key)
    {
      if(node->next != NULL)
        node->next->prev = node->prev;

      if(node->prev != NULL)
        node->prev->next = node->next;

      if(list->head == node || list->tail == node)
        list->head = list->tail = NULL;

      kfree(node, sizeof *node);
    }
  }

  return E_OK;
}
