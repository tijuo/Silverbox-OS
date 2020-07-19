#include <kernel/struct.h>
#include <kernel/error.h>
#include <kernel/dlmalloc.h>

/*
static unsigned long _hash_func( unsigned char *key, size_t keysize )
{
  unsigned long hash = 0;

  for( size_t i=0; i < keysize; i++ )
  {
    hash += key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}

/ * XXX: Warning numBuckets must be a prime number. Hash table must never be more than half full. * /

htable_t *htable_create( size_t numBuckets )
{
  htable_t *htable = malloc(sizeof(htable_t);

  if(!htable)
    return NULL;

  htable->buckets = calloc( numBuckets, sizeof( struct _KeyValPair ) );

  if( !htable->buckets )
  {
    free(htable);
    return NULL;
  }
  else
  {
    htable->numBuckets = numBuckets;

    return htable;
  }
}

/ * XXX: Warning: Keys and Values are not freed * /

void htable_destroy( htable_t *htable )
{
  if(!htable)
    return;

  if( htable->buckets )
  {
    free(htable->buckets);
    htable->buckets = NULL;
  }

  htable->numBuckets = 0;

  return 0;
}

/ * XXX: Warning: This assumes that the key will stay in memory. It does not
   copy the key! * /

int htable_insert( htable_t *htable, void *key, size_t keysize,
                   void *value, size_t valsize )
{
  struct _KeyValPair *pair;
  size_t hash_index;

  if( !htable || !key || keysize == 0 || (value == NULL && valsize != 0))
    return E_INVALID_ARG;
  else if(!htable->buckets || htable->numBuckets == 0)
    return E_FAIL;

  hash_index = _hash_func(key,keysize);

  for(int i=0; i < htable->numBuckets; i++)
  {
    pair = &htable->buckets[(hash_index + i) % htable->numBuckets];

    if( !pair->valid )
    {
      pair->pair.key = malloc(keysize);

      if( !pair->pair.key )
        return E_FAIL;

      memcpy(pair->pair.key, key, keysize);

      pair->pair.value = value;

      pair->pair.keysize = keysize;
      pair->pair.valsize = valsize;
      pair->valid = 1;

      return E_OK;
    }
  }

  return E_FAIL;
}

int htable_get(const htable_t *htable, void *key, size_t keysize,
               void **val, size_t *valsize )
{
  struct _KeyValPair *pair;
  size_t hash_index;

  if( htable == NULL || key == NULL )
    return E_INVALID_ARG;
  else if( htable->buckets == NULL || htable->numBuckets == 0 )
    return E_FAIL;

  hash_index = _hash_func(key,keysize);

  for(int i=0; i < htable->numBuckets; i++)
  {
    pair = &htable->buckets[(hash_index + i) % htable->numBuckets];

    if( pair->valid && pair->pair.keysize == keysize &&
        memcmp(key, pair->pair.key, keysize) == 0 )
    {
      if( val )
        *(int **)val = (int *)pair->pair.value;

      if( valsize )
        *valsize = pair->pair.valsize;

      return E_OK;
    }
  }

  return E_FAIL;
}
*/

static list_node_t *_create_list_node(int key, void *elem);
static tree_node_t *_create_tree_node(int key, void *elem);
static tree_node_t *_tree_find_node(tree_t *tree, int key);
static void _attempt_rebalance(tree_t *tree, tree_node_t *node);
static void _tree_free_nodes(tree_node_t *node);

list_node_t *_create_list_node(int key, void *elem)
{
  list_node_t *node = malloc(sizeof(list_node_t));

  if(!node)
    return NULL;

  node->key = key;
  node->elem = elem;
  node->next = node->prev = NULL;

  return node;
}

tree_node_t *_create_tree_node(int key, void *elem)
{
  tree_node_t *node = malloc(sizeof(tree_node_t));

  if(!node)
    return NULL;

  node->key = key;
  node->elem = elem;
  node->left = node->right = node->parent = NULL;

  return node;
}

int list_init(list_t *list)
{
  list->head = list->tail = NULL;
  return E_OK;
}

int list_destroy(list_t *list)
{
  list_node_t *next;

  for(list_node_t *ptr=list->head; ptr != NULL; ptr=next)
  {
    next = ptr->next;

    free(ptr);
  }

  list->head = list->tail = NULL;
  return E_OK;
}

int list_insert_head(list_t *list, int key, void *element)
{
  list_node_t *node = _create_list_node(key, element);

  if(node == NULL)
    return E_FAIL;

  if(list->head != NULL && list->tail != NULL)
  {
    list->head->prev = node;
    node->next = list->head;
  }

  list->head = node;
  return E_OK;
}

int list_insert_tail(list_t *list, int key, void *element)
{
  list_node_t *node = _create_list_node(key, element);

  if(node == NULL)
    return E_FAIL;

  if(list->head != NULL && list->tail != NULL)
  {
    list->tail->next = node;
    node->prev = list->tail;
  }

  list->tail = node;

  return E_OK;
}

int list_find_first(list_t *list, int key, void **elemPtr)
{
  for(list_node_t *node=list->head; node != NULL; node = node->next)
  {
    if(node->key == key)
    {
      if(elemPtr != NULL)
        *elemPtr = node->elem;

      return E_OK;
    }
  }

  return E_FAIL;
}

int list_find_last(list_t *list, int key, void **elemPtr)
{
  for(list_node_t *node=list->tail; node != NULL; node = node->prev)
  {
    if(node->key == key)
    {
      if(elemPtr != NULL)
        *elemPtr = node->elem;

      return E_OK;
    }
  }

  return E_FAIL;
}

int list_remove_first(list_t *list, int key)
{
  for(list_node_t *node=list->head; node != NULL; node = node->next)
  {
    if(node->key == key)
    {
      if(node->next != NULL)
        node->next->prev = node->prev;

      if(node->prev != NULL)
        node->prev->next = node->next;

      if(list->head == node || list->tail == node)
        list->head = list->tail = NULL;

      free(node);
      return E_OK;
    }
  }

  return E_FAIL;

}

int list_remove_last(list_t *list, int key)
{
  for(list_node_t *node=list->tail; node != NULL; node=node->prev)
  {
    if(node->key == key)
    {
      if(node->next != NULL)
        node->next->prev = node->prev;

      if(node->prev != NULL)
        node->prev->next = node->next;

      if(list->head == node || list->tail == node)
        list->head = list->tail = NULL;

      free(node);
      return E_OK;
    }
  }

  return E_FAIL;
}

int list_remove_all(list_t *list, int key)
{
  list_node_t *next;

  for(list_node_t *node=list->head; node != NULL; node=next)
  {
    next = node->next;

    if(node->key == key)
    {
      if(node->next != NULL)
        node->next->prev = node->prev;

      if(node->prev != NULL)
        node->prev->next = node->next;

      if(list->head == node || list->tail == node)
        list->head = list->tail = NULL;

      free(node);
    }
  }

  return E_OK;
}

int tree_init(tree_t *tree)
{
  tree->root = NULL;
  return E_OK;
}

void _tree_free_nodes(tree_node_t *node)
{
  if(node == NULL)
    return;
  else
  {
    _tree_free_nodes(node->left);
    _tree_free_nodes(node->right);
    free(node);
  }
}

int tree_destroy(tree_t *tree)
{
  _tree_free_nodes(tree->root);
  tree->root = NULL;

  return E_OK;
}

void _attempt_rebalance(tree_t *tree, tree_node_t *node)
{
  tree_node_t *x, *y;

  if(node->parent != NULL && node->parent->parent != NULL)
  {
    x = node->parent->parent;
    y = node->parent;

    if(y->left == node && y->right == NULL)
    {
      if(x->left == y && x->right == NULL) // LL imbalance, do single right rotation
      {
        //x->left = y->right; // not necessary (if tree is already balanced)
        y->right = x;
        y->parent = x->parent;
        x->parent = y;

        if(y->parent == NULL)
          tree->root = y;
      }
      else if(x->right == y && x->left == NULL) // RL imbalance, do double rotation
      {
        node->parent = x->parent;
        y->parent = x->parent = node;
        node->left = x;
        node->right = y;

        y->left = x->right = NULL;

       if(node->parent == NULL)
         tree->root = node;
      }
    }
    else if(y->right == node && y->left == NULL)
    {
      if(x->right == y && x->left == NULL) // RR imbalance, do single left rotation
      {
        //x->right = y->left; // not necessary (if tree is already balanced)
        y->left = x;
        y->parent = x->parent;
        x->parent = y;

        if(y->parent == NULL)
          tree->root = y;
      }
      else if(x->left == y && x->right == NULL) // LR imbalance, do double rotation
      {
        node->parent = x->parent;
        y->parent = x->parent = node;
        node->left = y;
        node->right = x;

        y->right = x->left = NULL;

       if(node->parent == NULL)
         tree->root = node;
      }
    }
  }
}

int tree_insert(tree_t *tree, int key, void *element)
{
  tree_node_t *node = _create_tree_node(key, element);

  if(node == NULL)
    return E_FAIL;
  else if(tree->root == NULL)
    tree->root = node;
  else
  {
    tree_node_t *ptr = tree->root;

    while(1)
    {
      if(key < ptr->key)
      {
        if(ptr->left == NULL)
        {
          ptr->left = node;
          node->parent = ptr;
          break;
        }
        else
          ptr = ptr->left;
      }
      else if(key > ptr->key)
      {
        if(ptr->right == NULL)
        {
          ptr->right = node;
          node->parent = ptr;
          break;
        }
        else
          ptr = ptr->right;
      }
      else
        return E_FAIL;
    }

    _attempt_rebalance(tree, node);
  }

  return E_OK;
}

tree_node_t *_tree_find_node(tree_t *tree, int key)
{
  tree_node_t *node=tree->root;

  while(node != NULL)
  {
    if(node->key == key)
      break;
    else if(key < node->key)
      node = node->left;
    else
      node = node->right;
  }

  return node;
}

int tree_find(tree_t *tree, int key, void **elemPtr)
{
  tree_node_t *node=_tree_find_node(tree, key);

  if(node)
  {
    if(elemPtr != NULL)
      *elemPtr = node->elem;
    return E_OK;
  }
  else
    return E_FAIL;
}

int tree_remove(tree_t *tree, int key)
{
  tree_node_t *node=_tree_find_node(tree, key);

  if(node)
  {
    if(!node->left && !node->right) // a leaf node. just remove it
    {
      if(node->parent == tree->root)
        tree->root = NULL;
      else if(node->parent->left == node)
        node->parent->left = NULL;
      else if(node->parent->right == node)
        node->parent->right = NULL;

      free(node);
    }
    else if((node->left && !node->right) || (!node->left && node->right)) // has one child. replace node with child and delete it
    {
      if(node->left)
      {
        if(!node->parent)
          tree->root = node->left;
        else if(node->parent->left == node)
          node->parent->left = node->left;
        else if(node->parent->right == node)
          node->parent->right = node->left;

        node->left->parent = node->parent;
      }
      else
      {
        if(!node->parent)
          tree->root = node->right;
        else if(node->parent->left == node)
          node->parent->left = node->right;
        else if(node->parent->right == node)
          node->parent->right = node->right;

        node->right->parent = node->parent;
      }

      free(node);
    }
    else //has two children. replace node with in-order successor (leftmost child in right tree)
    {
      tree_node_t *succ=node->right;

      while(succ->left)
        succ = succ->left;

      node->key = succ->key;
      node->elem = succ->elem;

      if(succ->parent->left == succ)
        succ->parent->left = NULL;
      if(succ->parent->right == succ)
        succ->parent->right = NULL;

      free(succ);
    }

    // XXX: Now, the tree needs to be rebalanced, if necessary for each ancestor

    return E_OK;
  }
  else
    return E_FAIL;
}
