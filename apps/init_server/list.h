#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdbool.h>

struct ListNode
{
  int key;
  void *element;
  struct ListNode *prev, *next;
};

struct ListType
{
  struct ListNode *head, *tail;
  void *(*malloc)(size_t);
  void (*free)(void *);
};

void list_init(struct ListType *list, void *(*malloc_func)(size_t), void (*free_func)(void *));
bool list_lookup(int key, struct ListType *list);
int list_get_element(int key, void **element_ptr, struct ListType *list);
int list_remove(int key, struct ListType *list);
int list_insert(int key, void *element, struct ListType *list);

#endif /* LIST_H */
