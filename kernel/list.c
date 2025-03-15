#include <kernel/debug.h>
#include <kernel/error.h>
#include <kernel/list.h>
#include <kernel/thread.h>

/**
 *  Insert a thread onto a list at the head or tail.
 *
 *  @param list The list onto which to push the thread. (Must not be NULL)
 *  @param thread The TCB of the thread. (Must not be NULL)
 *  @param atTail true, if the thread should be added at the tail of the list.
 * false, if it should be added at the head.
 */
NON_NULL_PARAMS void list_insert_at_end(list_t* list,
    tcb_t* thread,
    int at_tail)
{
    tid_t threadTid = get_tid(thread);

    if(LIST_IS_EMPTY(list)) {
        list->head_tid = threadTid;
        list->tail_tid = threadTid;
        thread->next_tid = NULL_TID;
        thread->prev_tid = NULL_TID;
    } else if(at_tail) {
        thread->next_tid = NULL_TID;
        tcb_t* t = get_tcb(list->tail_tid);

        if(t) {
            t->next_tid = threadTid;
        }

        thread->prev_tid = list->tail_tid;
        list->tail_tid = threadTid;
    } else {
        thread->prev_tid = NULL_TID;
        tcb_t* t = get_tcb(list->head_tid);

        if(t) {
            t->prev_tid = threadTid;
        }

        thread->next_tid = list->head_tid;
        list->head_tid = threadTid;
    }
}

/**
 *  Remove a thread from either the head or tail of a list.
 *
 *  @param list The list from which to remove a thread.
 *  @param atTail true, if the thread should be removed from the tail of the
 * list. false, if it should be removed from the head.
 *  @return The removed thread's TCB on success. NULL, if the list is empty.
 */
NON_NULL_PARAMS tcb_t* list_remove_from_end(list_t* list, int at_tail)
{
    tcb_t* removed_tcb = get_tcb(at_tail ? list->tail_tid : list->head_tid);

#ifdef DEBUG
    if(!list->head_tid || !list->tail_tid)
        kassert(list->head_tid == NULL_TID && list->tail_tid == NULL_TID);

    if(list->head_tid || list->tail_tid)
        kassert(list->head_tid != NULL_TID && list->tail_tid != NULL_TID);
#endif /* DEBUG */

    if(removed_tcb) {
        if(at_tail) {
            list->tail_tid = removed_tcb->prev_tid;

            if(list->tail_tid == NULL_TID)
                list->head_tid = NULL_TID;
            else
                get_tcb(list->tail_tid)->next_tid = NULL_TID;
        } else {
            list->head_tid = removed_tcb->next_tid;

            if(list->head_tid == NULL_TID)
                list->tail_tid = NULL_TID;
            else
                get_tcb(list->head_tid)->prev_tid = NULL_TID;
        }

        removed_tcb->prev_tid = NULL_TID;
        removed_tcb->next_tid = NULL_TID;

        return removed_tcb;
    } else
        return NULL;
}

/**
 *  Remove a thread from its current position in a list. This function assumes
 *  that the thread is actually in the list.
 *
 *  @param list The list from which the thread will be removed. (Must not be
 * null)
 *  @param thread The TCB of the thread to be removed. (Must not be null)
 */
NON_NULL_PARAMS void list_remove(list_t* list, tcb_t* thread)
{
    tid_t thread_tid = get_tid(thread);

    if(thread->next_tid != NULL_TID)
        get_tcb(thread->next_tid)->prev_tid = thread->prev_tid;

    if(thread->prev_tid != NULL_TID)
        get_tcb(thread->prev_tid)->next_tid = thread->next_tid;

    if(list->head_tid == thread_tid)
        list->head_tid = thread->next_tid;

    if(list->tail_tid == thread_tid)
        list->tail_tid = thread->prev_tid;

    thread->prev_tid = NULL_TID;
    thread->next_tid = NULL_TID;
}
