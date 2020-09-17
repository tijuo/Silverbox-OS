#ifndef KERNEL_LIST_H
#define KERNEL_LIST_H

#include <kernel/list_struct.h>
#include <kernel/thread.h>

typedef struct Node
{
  union {
    struct Node *prev;
    int delta;
  };
  struct Node *next;
} node_t;

extern node_t threadNodes[];

/*
int listPush(list_t *list, tcb_t *thread);
tcb_t *listPop(list_t *list);
*/

int listEnqueue(list_t *list, tcb_t *thread);
tcb_t *listDequeue(list_t *list);
int listRemove(list_t *list, tcb_t *thread);

int deltaListPush(list_t *list, tcb_t *thread, int delta);
tcb_t *deltaListPop(list_t *list);
int deltaListRemove(list_t *list, tcb_t *thread);

node_t *getHeadNode(list_t *list);
node_t *getTailNode(list_t *tail);

#define isListEmpty(list)       ((list)->headTid == NULL_TID)
#endif /* KERNEL_LIST_H */
