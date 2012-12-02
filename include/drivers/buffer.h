#ifndef BUFFER_CACHE
#define BUFFER_CACHE

struct buffer {
  int lock;
  void *data;
  unsigned long block;
  size_t blkSize;
  int device;
  unsigned char dirty : 1;
  unsigned char accessed : 1;
  unsigned useCount;
};

#endif
