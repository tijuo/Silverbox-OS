#ifndef KERNEL_LIST_H
#define KERNEL_LIST_H

#include <kernel/list_struct.h>
#include <kernel/thread.h>
#include <util.h>

NON_NULL_PARAMS void list_insert_at_end(list_t *list, tcb_t *thread, int at_tail);
NON_NULL_PARAMS tcb_t* list_remove_from_end(list_t *list, int at_tail);
NON_NULL_PARAMS void list_remove(list_t *list, tcb_t *thread);

//int listEnqueue(list_t *list, tcb_t *thread);
//tcb_t *listDequeue(list_t *list);
//int listRemove(list_t *list, tcb_t *thread);

// Inserts a thread at the head of a list.
#define list_enqueue(list, thread)   list_insert_at_end(list, thread, 0)

// Removes a thread from the tail of a list
#define list_dequeue(list)           list_remove_from_end(list, 1)

// Pushes a thread to the head of the list
#define list_push(list, thread)      list_insert_at_end(list, thread, 0)

// Pops a thread from the head of the list
#define list_pop(list)               list_remove_from_end(list, 0)

NON_NULL_PARAMS static inline bool is_list_empty(list_t *list) {
  return list->head_tid == NULL_TID;
}

#endif /* KERNEL_LIST_H */
