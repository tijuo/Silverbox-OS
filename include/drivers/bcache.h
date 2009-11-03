#ifndef BLOCK_CACHE
#define BLOCK_CACHE

struct BlockData
{
  int lock;
  void *data;
  unsigned blockNum;
  unsigned blockSize;
  int deviceNum;
  bool dirty;
};

#endif
