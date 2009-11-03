enum TreeColors { BLACK, RED };

struct TreeNode
{
  int key;

  union
  {
    char balance;
    char color;
  };

  void *element;
  struct TreeNode *parent, *left, *right;
};

struct TreeType
{
  struct TreeNode *root;
  void *(*malloc)(size_t);
  void (*free)(void *);  
};

int tree_lookup(int key, struct TreeType *tree);
int tree_get_element(int key, void **element_ptr, struct TreeType *tree);
int tree_remove(key, struct TreeType *tree);
int tree_insert(int key, void *element, struct TreeType *tree);

int tree_insert(int key, void *element, struct TreeType *tree)
{
  struct TreeNode *node;

  if( tree == NULL)
    return -1;

  node = tree->malloc(sizeof(struct TreeNode));

  if( node == NULL )
    return -1;

  node->balance = 0;
  node->key = key;
  node->left = node->right = node->parent = NULL;
  node->element = element;

  if( tree->root == NULL )
    tree->root = node;
  
}

int tree_remove(key, struct TreeType *tree)
{

}

int tree_get_element(int key, void **element_ptr, struct TreeType *tree)
{
  struct TreeNode *node;

  if( tree == NULL )
    return -1;

  node = tree->root;

  while( node != NULL )
  {
    if( key < node->key )
      node = node->left;
    else if(key > node->key)
      node = node->right;
    else
    {
      if(element_ptr != NULL )
        *(unsigned long)element_ptr = (unsigned long)node->element;
      return 0;
    }
  }
  return -1;
}

int tree_lookup(int key, struct TreeType *tree)
{
  return tree_get_element(key, NULL, tree);
}
