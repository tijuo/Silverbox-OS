#define getTreeNodeElem( node, type ) (*((type *)(node->elem)))

typedeft struct {
  void *elem;
  int key;
  TreeNode *left;
  TreeNode *right;
  TreeNode *prev;
} TreeNode;

bool hasLeft(TreeNode *node) {
  if(node->left != NULL)
    return true;
  else
    return false;
}

bool hasRight(TreeNode *node) {
  if(node->right != NULL)
    return true;
  else
    return false;
}

TreeNode* insertTree(TreeNode *node, TreeNode *tree) {
  TreeNode * ptr;

  if(tree == NULL)
    tree = node;
  else {
    ptr = tree;

    while(true) {
      if(node->key > ptr) {
        if(ptr->right == NULL) {
          ptr->right = NULL;
          break;
        }
        else
          ptr = ptr->right;
      }
      else if(node->key >= ptr) {
        if(ptr->left == NULL) {
          ptr->left = NULL;
          break;
        }
        else
          ptr = ptr->left;
      }
    }
  }

  return node;
}

TreeNode* removeTree(TreeNode *node, TreeNode *tree) {
  if(node->left == NULL && node->right == NULL)
    node->right = NULL;
  else if(node->left == NULL && node->right != NULL)
    node->right = node->right;
  else if(node->left != NULL && node->right == NULL)
    node->right = node->left;
  else if(node->left != NULL && node->right != NULL) {
    TreeNode *predecessor = node->left;

    if(predecessor->right == NULL) {
      predecessor->right = node->right;
      node->prev->right = predecessor;
    }
    else {
      while(predecessor->right != NULL) {
        if(predecessor->right->right == NULL) {
          predecessor->right->right = node->right;
          predecessor->right->left = node->left;
          node->prev->right = predecessor->right;
          predecessor->right = NULL;
          break;
        }

        predecessor = predecessor->right;
      }
    }
  }
  return node;
}
