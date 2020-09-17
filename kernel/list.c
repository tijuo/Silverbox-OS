#include <kernel/list.h>
#include <kernel/thread.h>
#include <kernel/error.h>
#include <kernel/debug.h>

#define GET_THREAD_NODE(thread)     (&threadNodes[getTid(thread)])

node_t threadNodes[MAX_THREADS];

static int _listInsertAtEnd(list_t *list, tcb_t *thread, int isDelta, int timeDelta, int atTail);
static tcb_t *_listRemoveFromEnd(list_t *list, int isDelta, int atTail);
static int _listRemove(list_t *list, tcb_t *thread, int isDelta);
static inline tid_t getNodeTid(node_t *node);

static inline tid_t getNodeTid(node_t *node)
{
  return node ? (tid_t)(node - threadNodes) : NULL_TID;
}

/**
 *  Insert a thread onto a list at the head or tail. If the list is a delta list, then
 *  the thread will be inserted into the list according to a specified time delta instead.
 *
 *  @param list The list onto which to push the thread.
 *  @param thread The TCB of the thread.
 *  @param isDelta Is the list a delta list?
 *  @param timeDelta The time delta for the thread to be added (if applicable).
 *  @param atTail true, if the thread should be added at the tail of the list. false, if it
 *         should be added at the head.
 *  @return E_OK on success. E_INVALID_ARG if thread is NULL.
 */

static int _listInsertAtEnd(list_t *list, tcb_t *thread, int isDelta, int timeDelta, int atTail)
{
  assert(list);
  assert(thread);

  if(!thread)
    RET_MSG(E_INVALID_ARG, "Thread is null.");
  else
  {
    node_t *threadNode = GET_THREAD_NODE(thread);
    tid_t threadTid = getTid(thread);

    if(isDelta)
    {
      assert(timeDelta > 0);

      if((atTail ? list->tailTid : list->headTid) == NULL_TID)
      {
        list->headTid = threadTid;
        list->tailTid = threadTid;
        threadNode->next = NULL;
      }
      else if(atTail)
      {
        threadNodes[list->tailTid].next = threadNode;
        threadNode->next = NULL;
        list->tailTid = threadTid;
      }
      else
      {
        int stopped=0;

        node_t *ptr = list->headTid == NULL_TID ? NULL : &threadNodes[list->headTid];

        for(node_t *prevPtr=NULL; ptr; prevPtr=ptr, ptr=ptr->next)
        {
          if(ptr->delta <= timeDelta)
            timeDelta -= ptr->delta;
          else
          {
            stopped = 1;

            threadNode->next = ptr;
            ptr->delta -= timeDelta;

            if(prevPtr)
              prevPtr->next = threadNode;
            else
              list->headTid = threadTid;

            break;
          }
        }

        if(!stopped)
        {
          if(list->tailTid == NULL_TID)
            list->headTid = threadTid;
          else
            threadNodes[list->tailTid].next = threadNode;

          list->tailTid = threadTid;
        }
      }

      threadNode->delta = timeDelta;
    }
    else
    {
      if(atTail)
      {
        threadNode->next = NULL;
        threadNodes[list->tailTid].next = threadNode;
        threadNode->prev = &threadNodes[list->tailTid];
        list->tailTid = threadTid;

        if(list->headTid == NULL_TID)
          list->headTid = list->tailTid;
      }
      else
      {
        threadNode->prev = NULL;
        threadNodes[list->headTid].prev = threadNode;
        threadNode->next = &threadNodes[list->headTid];
        list->headTid = threadTid;

        if(list->tailTid == NULL_TID)
          list->tailTid = list->headTid;
      }
    }
  }

  return E_OK;
}

/**
 *  Remove a thread from either the head or tail of a list.
 *
 *  @param list The list from which to remove a thread.
 *  @param isDelta Is the list a delta list?
 *  @param atTail true, if the thread should be removed from the tail of the list. false, if it
 *         should be removed from the head.
 *  @return The removed thread's TCB on success. NULL, if the list is empty.
 */

static tcb_t *_listRemoveFromEnd(list_t *list, int isDelta, int atTail)
{
  assert(list);
  tid_t removedTid = atTail ? list->tailTid : list->headTid;

  if(removedTid != NULL_TID)
  {
    node_t *removedNode = &threadNodes[removedTid];

#if DEBUG
    if(isDelta)
    {
      if(removedNode->delta < 0)
        kprintf("Removed node delta: %d\n", removedNode->delta);

      assert(removedNode->delta >= 0);
    }
#endif /* DEBUG */

    if(atTail)
    {
      list->tailTid = getNodeTid(removedNode->prev);

      if(!isDelta)
      {
        if(list->tailTid != NULL_TID)
          threadNodes[list->tailTid].next = NULL;

        removedNode->next = NULL;
      }

      if(list->tailTid == NULL_TID)
        list->headTid = NULL_TID;

      removedNode->prev = NULL;
    }
    else
    {
      list->headTid = getNodeTid(removedNode->next);

      if(!isDelta)
      {
        if(list->headTid != NULL_TID)
        {
          threadNodes[list->headTid].prev = NULL;
          threadNodes[list->headTid].delta += removedNode->delta;
        }

        removedNode->prev = NULL;
      }

      if(list->headTid == NULL_TID)
        list->tailTid = NULL_TID;

      removedNode->next = NULL;
    }

    return getTcb(removedTid);
  }
  else
    RET_MSG(NULL, "List is empty.");
}

/**
 *  Remove a thread from its current position in a list. This function assumes
 *  that the thread is actually in the list.
 *
 *  @param list The list from which the thread will be removed.
 *  @param thread The TCB of the thread to be removed.
 *  @param isDelta Is the list a delta list?
 *  @return E_OK on success. E_INVALID_ARG if thread is NULL or invalid. E_FAIL if the thread was not
 *          able to be removed.
 */

static int _listRemove(list_t *list, tcb_t *thread, int isDelta)
{
  assert(list);
  assert(thread);

  tid_t threadTid = getTid(thread);

  if(!thread || threadTid == NULL_TID)
    RET_MSG(E_INVALID_ARG, "Invalid thread.");

  node_t *threadNode = GET_THREAD_NODE(thread);

  if(isDelta)
  {
    assert(threadNode->delta >= 0);
    node_t *ptr = list->headTid == NULL_TID ? NULL : &threadNodes[list->headTid];

    if(list->headTid == threadTid)
    {
      if(ptr->next)
        ptr->next->delta += threadNode->delta;
      else
        list->tailTid = NULL_TID;

      list->headTid = getNodeTid(ptr->next);
    }
    else
    {
      int found = 0;

      for(; ptr; ptr=ptr->next)
      {
        if(ptr->next == threadNode)
        {
          found = 1;
          ptr->next = threadNode->next;

          if(threadNode->next)
          {
            assert(threadNode->next->delta >= 0);
            threadNode->next->delta += threadNode->delta;
          }

          if(list->tailTid == threadTid)
            list->tailTid = getNodeTid(ptr);
        }
      }

      if(!found)
        RET_MSG(E_FAIL, isDelta ? "Thread is not in delta list." : "Thread is not in list.");
    }
  }
  else
  {
    if(threadNode->next)
      threadNode->next->prev = threadNode->prev;

    if(threadNode->prev)
      threadNode->prev->next = threadNode->next;

    if(list->headTid == threadTid)
      list->headTid = getNodeTid(threadNode->next);

    if(list->tailTid == threadTid)
      list->tailTid = getNodeTid(threadNode->prev);
  }

  threadNode->prev = NULL;
  threadNode->next = NULL;

  return E_OK;
}

/**
 *  Inserts a thread at the head of a list.
 *
 *  @param list The list onto which to push the thread.
 *  @param thread The TCB of the thread.
 *  @return E_OK on success. E_INVALID_ARG if thread is NULL.
 */

int listEnqueue(list_t *list, tcb_t *thread)
{
  return _listInsertAtEnd(list, thread, 0, 0, 0);
}

/**
 *  Remove a thread from the head of a list.
 *
 *  @param list The list from which to pop the thread.
 *  @return The popped thread's TCB on success. NULL, if the list is empty.
 */

tcb_t *listDequeue(list_t *list)
{
  return _listRemoveFromEnd(list, 0, 1);
}

/**
 *  Remove a thread from its current position in a list. This function assumes
 *  that the thread is actually in the list.
 *
 *  @param list The list from which the thread will be removed.
 *  @param thread The TCB of the thread to be removed.
 *
 *  @return E_OK on success. E_INVALID_ARG if thread is NULL.
 */

int listRemove(list_t *list, tcb_t *thread)
{
  return _listRemove(list, thread, 0);
}

/**
 *  Push a thread onto a delta list  according to a specified time delta.
 *
 *  @param list The delta list onto which to push the thread.
 *  @param thread The TCB of the thread.
 *  @param timeDelta The time delta for the thread to be added (if applicable).
 *  @return E_OK on success. E_INVALID_ARG if thread is NULL.
 */

int deltaListPush(list_t *list, tcb_t *thread, int delta)
{
  return _listInsertAtEnd(list, thread, 1, delta, 0);
}

/**
 *  Detach the thread that was most recently pushed to a delta list.
 *
 *  @param list The delta list from which to pop a thread.
 *  @return The popped thread's TCB on success. NULL, if the list is empty.
 */

tcb_t *deltaListPop(list_t *list)
{
  return _listRemoveFromEnd(list, 1, 0);
}

/**
 *  Remove a thread from its current position in a delta list. This function assumes
 *  that the thread is actually in the list.
 *
 *  @param list The delta list from which the thread will be removed.
 *  @param thread The TCB of the thread to be removed.
 *  @return E_OK on success. E_INVALID_ARG if thread is NULL.
 */

int deltaListRemove(list_t *list, tcb_t *thread)
{
  return _listRemove(list, thread, 1);
}


node_t *getHeadNode(list_t *list)
{
  return list->headTid == NULL_TID ? NULL : &threadNodes[list->headTid];
}

node_t *getTailNode(list_t *list)
{
  return list->tailTid == NULL_TID ? NULL : &threadNodes[list->tailTid];
}
