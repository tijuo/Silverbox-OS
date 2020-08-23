#ifndef SBARRAY_H
#define SBARRAY_H

#include <stddef.h>
#include <os/ostypes/ostypes.h>

typedef struct SBArray
{
  const void **elems;
  int nElems;
  int capacity;
} sbarray_t;

int sbArrayClear(sbarray_t *);
sbarray_t *sbArrayCopy(const sbarray_t *array, sbarray_t *newArray);
size_t sbArrayCount(const sbarray_t *array);
int sbArrayCreate(sbarray_t *array);
void sbArrayDelete(sbarray_t *array);
int sbArrayGet(const sbarray_t *array, int pos, const void **elem);
int sbArrayFind(const sbarray_t *array, const void *elem);
int sbArrayInsert(sbarray_t *array, int pos, const void *ptr);
int sbArrayPop(sbarray_t *array, const void **elem);
int sbArrayPush(sbarray_t *array, const void *ptr);
int sbArrayRemove(sbarray_t *array, int pos);
int sbArraySlice(const sbarray_t *array, int start, int end, sbarray_t *newArray);

#endif /* SBARRAY_H */
