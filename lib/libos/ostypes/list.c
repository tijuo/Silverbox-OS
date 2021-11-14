#include <os/list.h>
#include <stdlib.h>

enum ListOpWhich {
  FIRST, LAST, ALL, HEAD, TAIL
};

static list_node_t* _createListNode(int key, void *elem);
static int _listInsert(list_t *list, int key, void *element,
                       enum ListOpWhich which);
static int _listFind(list_t *list, int key, void **elemPtr,
                     enum ListOpWhich which);
static int _listRemove(list_t *list, int key, void **elemPtr,
                       enum ListOpWhich which);
static int _listRemoveEnd(list_t *list, void **elemPtr, enum ListOpWhich which);

static list_node_t* _createListNode(int key, void *elem) {
  list_node_t *node = kmalloc(sizeof(list_node_t));

  if(!node)
    return NULL;

  node->key = key;
  node->elem = elem;
  node->next = node->prev = NULL;

  return node;
}

/** Initialize a new list.
 *
 * @param list The list to initialize (assumed to be non-null).
 * @return 0 on success.
 */

int listInit(list_t *list) {
  list->head = list->tail = NULL;
  return 0;
}

/** Destroys a list by freeing any memory allocated for nodes.
 *
 * @param list The list to destroy (assumed to be non-null).
 * @return 0 on success.
 */

int listDestroy(list_t *list) {
  list_node_t *next;

  for(list_node_t *ptr = list->head; ptr != NULL; ptr = next) {
    next = ptr->next;
    kfree(ptr, sizeof *ptr);
  }

  list->head = list->tail = NULL;
  return 0;
}

static int _listInsert(list_t *list, int key, void *element,
                       enum ListOpWhich which)
{
  list_node_t *node = _createListNode(key, element);

  if(!node)
    return -1;

  if(which == HEAD) {
    if(list->head) {
      list->head->prev = node;
      node->next = list->head;
    }

    list->head = node;

    if(!list->tail)
      list->tail = node;
  }
  else {
    if(list->tail) {
      list->tail->next = node;
      node->prev = list->tail;
    }

    list->tail = node;

    if(!list->head)
      list->head = node;
  }

  return 0;
}

int listInsertHead(list_t *list, int key, void *element) {
  return _listInsert(list, key, element, HEAD);
}

int listInsertTail(list_t *list, int key, void *element) {
  return _listInsert(list, key, element, TAIL);
}

static int _listFind(list_t *list, int key, void **elemPtr,
                     enum ListOpWhich which)
{
  list_node_t *node = (which == FIRST) ? list->head : list->tail;

  while(node) {
    if(node->key == key) {
      if(elemPtr != NULL)
        *elemPtr = node->elem;

      return 0;
    }

    node = (which == FIRST) ? node->next : node->prev;
  }

  return -1;
}

int listFindFirst(list_t *list, int key, void **elemPtr) {
  return _listFind(list, key, elemPtr, FIRST);
}

int listFindLast(list_t *list, int key, void **elemPtr) {
  return _listFind(list, key, elemPtr, LAST);
}

static int _listRemoveEnd(list_t *list, void **elemPtr, enum ListOpWhich which)
{
  list_node_t *prevNode = (which == HEAD) ? list->head : list->tail;

  if(prevNode) {
    if(which == HEAD) {
      list->head = prevNode->next;

      if(list->head)
        list->head->prev = NULL;
      else
        list->tail = NULL;
    }
    else {
      list->tail = prevNode->prev;

      if(list->tail)
        list->tail->next = NULL;
      else
        list->head = NULL;
    }

    if(elemPtr)
      *elemPtr = prevNode->elem;

    kfree(prevNode, sizeof *prevNode);
    return 0;
  }
  else
    return -1;
}

int listRemoveHead(list_t *list, void **elemPtr) {
  return _listRemoveEnd(list, elemPtr, HEAD);
}

int listRemoveTail(list_t *list, void **elemPtr) {
  return _listRemoveEnd(list, elemPtr, TAIL);
}

static int _listRemove(list_t *list, int key, void **elemPtr,
                       enum ListOpWhich which)
{
  list_node_t *node = (which == LAST) ? list->tail : list->head;

  while(node) {
    if(node->key == key) {
      if(node->next)
        node->next->prev = node->prev;

      if(node->prev)
        node->prev->next = node->next;

      if(list->head == node)
        list->head = node->next;

      if(list->tail == node)
        list->tail = node->prev;

      if(elemPtr)
        *elemPtr = node->elem;

      kfree(node, sizeof *node);

      if(which != ALL)
        return 0;
    }

    node = (which == LAST) ? node->prev : node->next;
  }

  return (which == ALL) ? 0 : -1;
}

int listRemoveFirst(list_t *list, int key, void **elemPtr) {
  return _listRemove(list, key, elemPtr, FIRST);
}

int listRemoveLast(list_t *list, int key, void **elemPtr) {
  return _listRemove(list, key, elemPtr, LAST);
}

int listRemoveAll(list_t *list, int key) {
  return _listRemove(list, key, NULL, ALL);
}
