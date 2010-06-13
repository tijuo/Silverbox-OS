#ifndef SBARRAY_H
#define SBARRAY_H

#include <stddef.h>

enum SBArrayValues { SBArrayError=-1, SBArrayNotFound=-2, SBArrayFailed=-3, 
       SBArrayEmpty=-4 };

struct _SBArrayElem
{
  void *ptr;
  size_t size;
};

struct _SBArray
{
  struct _SBArrayElem *elems;
  int nElems;
  int capacity;
};

typedef struct _SBArray SBArray;

int sbArrayClear(SBArray *);
int sbArrayCopy(const SBArray *array, SBArray *newArray);
int sbArrayCount(const SBArray *array);
//int sbArrayCreate(SBArray *array, int numElems, ...);
int sbArrayCreate(SBArray *array);
int sbArrayDelete(SBArray *array);
int sbArrayElemAt(const SBArray *array, int pos, void **elem, size_t *size);
int sbArrayFind(const SBArray *array, void *elem, size_t size);
int sbArrayInsert(SBArray *array, int pos, void *ptr, size_t size);
int sbArrayPop(SBArray *array, void **ptr, size_t *size);
int sbArrayPush(SBArray *array, void *ptr, size_t size);
int sbArrayRemove(SBArray *array, int pos);
int sbArraySlice(const SBArray *array, int start, int end, SBArray *newArray);

#endif /* SBARRAY_H */
