#include <kernel/list.h>
#include <kernel/thread.h>
#include <kernel/error.h>
#include <kernel/debug.h>

/**
 *  Insert a thread onto a list at the head or tail.
 *
 *  @param list The list onto which to push the thread. (Must not be NULL)
 *  @param thread The TCB of the thread. (Must not be NULL)
 *  @param atTail true, if the thread should be added at the tail of the list. false, if it
 *         should be added at the head.
 */

NON_NULL_PARAMS void listInsertAtEnd(list_t *list, tcb_t *thread, int atTail) {
  tid_t threadTid = getTid(thread);

  if(isListEmpty(list)) {
    list->headTid = threadTid;
    list->tailTid = threadTid;
    thread->nextTid = NULL_TID;
    thread->prevTid = NULL_TID;
  }
  else if(atTail) {
    thread->nextTid = NULL_TID;
    getTcb(list->tailTid)->nextTid = threadTid;
    thread->prevTid = list->tailTid;
    list->tailTid = threadTid;
  }
  else {
    thread->prevTid = NULL_TID;
    getTcb(list->headTid)->prevTid = threadTid;
    thread->nextTid = list->headTid;
    list->headTid = threadTid;
  }
}

/**
 *  Remove a thread from either the head or tail of a list.
 *
 *  @param list The list from which to remove a thread.
 *  @param atTail true, if the thread should be removed from the tail of the list. false, if it
 *         should be removed from the head.
 *  @return The removed thread's TCB on success. NULL, if the list is empty.
 */

NON_NULL_PARAMS tcb_t* listRemoveFromEnd(list_t *list, int atTail) {
  tcb_t *removedTcb = getTcb(atTail ? list->tailTid : list->headTid);

#ifdef DEBUG
  if(!list->headTid || !list->tailTid)
    assert(list->headTid == NULL_TID && list->tailTid == NULL_TID);

  if(list->headTid || list->tailTid)
    assert(list->headTid != NULL_TID && list->tailTid != NULL_TID);
#endif /* DEBUG */

  if(removedTcb) {
    if(atTail) {
      list->tailTid = removedTcb->prevTid;

      if(list->tailTid == NULL_TID)
        list->headTid = NULL_TID;
      else
        getTcb(list->tailTid)->nextTid = NULL_TID;
    }
    else {
      list->headTid = removedTcb->nextTid;

      if(list->headTid == NULL_TID)
        list->tailTid = NULL_TID;
      else
        getTcb(list->headTid)->prevTid = NULL_TID;
    }

    removedTcb->prevTid = NULL_TID;
    removedTcb->nextTid = NULL_TID;

    return removedTcb;
  }
  else
    return NULL;
}

/**
 *  Remove a thread from its current position in a list. This function assumes
 *  that the thread is actually in the list.
 *
 *  @param list The list from which the thread will be removed. (Must not be null)
 *  @param thread The TCB of the thread to be removed. (Must not be null)
 */

void listRemove(list_t *list, tcb_t *thread) {
  assert(list);
  assert(thread);

  tid_t threadTid = getTid(thread);

  if(thread->nextTid != NULL_TID)
    getTcb(thread->nextTid)->prevTid = thread->prevTid;

  if(thread->prevTid != NULL_TID)
    getTcb(thread->prevTid)->nextTid = thread->nextTid;

  if(list->headTid == threadTid)
    list->headTid = thread->nextTid;

  if(list->tailTid == threadTid)
    list->tailTid = thread->prevTid;

  thread->prevTid = NULL_TID;
  thread->nextTid = NULL_TID;
}
