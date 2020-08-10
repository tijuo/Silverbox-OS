struct Dentry
{
  char name[63];
  char nameLen;
  char attrib;
  size_t size;
  time_t timestamp;
  struct BTreeNode *dataNodes; // either files or directories
};

struct BTreeNode
{
  int blocks[5];
  struct BTreeNode *nodes[3];
};

struct RamFsHeader
{
  size_t blockSize;
  unsigned numBlocks;
  struct Dentry *rootDentry;
};
