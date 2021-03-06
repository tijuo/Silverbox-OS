#ifndef LIST_H
#define LIST_H

struct ListNode
{
  int key;
  void *elem;
  struct ListNode *prev;
  struct ListNode *next;
};

typedef struct ListNode list_node_t;

struct List
{
  list_node_t *head;
  list_node_t *tail;
};

typedef struct List list_t;

int listInit(list_t *list);
int listDestroy(list_t *list);
int listInsertHead(list_t *list, int key, void *element);
int listInsertTail(list_t *list, int key, void *element);
int listFindFirst(list_t *list, int key, void **elemPtr);
int listFindLast(list_t *list, int key, void **elemPtr);
int listRemoveFirst(list_t *list, int key, void **elemPtr);
int listRemoveLast(list_t *list, int key, void **elemPtr);
int listRemoveAll(list_t *list, int key);
int listRemoveHead(list_t *list, void **elemPtr);
int listRemoveTail(list_t *list, void **elemPtr);

#define queue_t			    list_t
#define queueInit		    listInit
#define queueEnqueue		listInsertHead
#define queueDequeue		listRemoveTail
#define queueRemoveFirst	listRemoveFirst
#define queueRemoveLast		listRemoveLast
#define queueDestroy		listDestroy
#define queueFindFirst		listFindFirst
#define queueFindLast		listFindLast
#define queueClear		    listClear
#define queuePush		    listInsertHead
#define queuePop		    listRemoveHead

#define listGetHead(l)		({ __typeof__ (l) _l=(l); isListEmpty(_l) ? NULL : _l->head->elem; })
#define listGetTail(l)		({ __typeof__ (l) _l=(l); isListEmpty(_l) ? NULL : _l->tail->elem; })

#define queueGetHead(q)		listGetHead(q)
#define queueGetTail(q)		listGetTail(q)

#define treeGetRoot(t)		({ __typeof__ (t) _t=(t); isTreeEmpty(_t) ? NULL : _t->root->elem; })

#define isListEmpty(l)		(!(l)->head || !(l)->tail)
#define isQueueEmpty(q)		isListEmpty(q)

#endif /* LIST_H */
