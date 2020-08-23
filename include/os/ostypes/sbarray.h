#ifndef SBARRAY_H
#define SBARRAY_H

#include <stddef.h>
#include <os/ostypes/ostypes.h>

typedef struct SBArray
{
  void **elems;
  int nElems;
  int capacity;
} sbarray_t;

int sbArrayClear(sbarray_t *);
sbarray_t *sbArrayCopy(sbarray_t *array, sbarray_t *newArray);
size_t sbArrayCount(sbarray_t *array);
int sbArrayCreate(sbarray_t *array);
void sbArrayDelete(sbarray_t *array);
int sbArrayGet(sbarray_t *array, int pos, void **elem);
int sbArrayFind(sbarray_t *array, void *elem);
int sbArrayInsert(sbarray_t *array, int pos, void *ptr);
int sbArrayPop(sbarray_t *array, void **elem);
int sbArrayPush(sbarray_t *array, void *ptr);
int sbArrayRemove(sbarray_t *array, int pos);
int sbArraySlice(sbarray_t *array, int start, int end, sbarray_t *newArray);

#endif /* SBARRAY_H */
