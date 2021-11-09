#ifndef KERNEL_LIST_H
#define KERNEL_LIST_H

#include <kernel/list_struct.h>
#include <kernel/thread.h>

void listInsertAtEnd(list_t *list, tcb_t *thread, int atTail);
tcb_t *listRemoveFromEnd(list_t *list, int atTail);
void listRemove(list_t *list, tcb_t *thread);

//int listEnqueue(list_t *list, tcb_t *thread);
//tcb_t *listDequeue(list_t *list);
//int listRemove(list_t *list, tcb_t *thread);

// Inserts a thread at the head of a list.
#define listEnqueue(list, thread)   listInsertAtEnd(list, thread, 0)

// Removes a thread from the tail of a list
#define listDequeue(list)           listRemoveFromEnd(list, 1)

// Pushes a thread to the head of the list
#define listPush(list, thread)      listInsertAtEnd(list, thread, 0)

// Pops a thread from the head of the list
#define listPop(list)               listRemoveFromEnd(list, 0)

#define isListEmpty(list)       ((list)->headTid == NULL_TID)
#endif /* KERNEL_LIST_H */
