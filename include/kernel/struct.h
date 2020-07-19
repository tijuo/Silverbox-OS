#ifndef STRUCT_H
#define STRUCT_H

struct ListNode
{
  int key;
  void *elem;
  struct ListNode *next, *prev;
};

struct TreeNode
{
  int key;
  void *elem;
  struct TreeNode *left, *right, *parent;
};

typedef struct ListNode list_node_t;
typedef struct TreeNode tree_node_t;

struct List
{
  list_node_t *head, *tail;
};

struct Tree
{
  tree_node_t *root;
};

typedef struct List list_t;
typedef struct Tree tree_t;

int list_init(list_t *list);
int list_destroy(list_t *list);
int list_insert_head(list_t *list, int key, void *element);
int list_insert_tail(list_t *list, int key, void *element);
int list_find_first(list_t *list, int key, void **elemPtr);
int list_find_last(list_t *list, int key, void **elemPtr);
int list_remove_first(list_t *list, int key);
int list_remove_last(list_t *list, int key);
int list_remove_all(list_t *list, int key);

int tree_init(tree_t *tree);
int tree_destroy(tree_t *tree);
int tree_insert(tree_t *tree, int key, void *element);
int tree_find(tree_t *tree, int key, void **elemPtr);
int tree_remove(tree_t *tree, int key);

#endif /* STRUCT_H */
