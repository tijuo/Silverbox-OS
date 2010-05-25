#ifndef SBARRAY_H
#define SBARRAY_H

enum SBArrayValues { SBArrayError=-1, SBArrayNotFound=-2, SBArrayFailed=-3, 
       SBArrayEmpty=-4 };

struct _SBArray
{
  int **ptrs;
  int nElems;
  int capacity;
};

typedef struct _SBArray SBArray;

int sbArrayClear(SBArray *);
int sbArrayCopy(const SBArray *array, SBArray *newArray);
int sbArrayCount(const SBArray *array);
int sbArrayCreate(SBArray *array, int numElems, ...);
int sbArrayDelete(SBArray *array);
int sbArrayElemAt(const SBArray *array, int pos, void **elem);
int sbArrayFind(const SBArray *array, void *elem);
int sbArrayInsert(SBArray *array, void *ptr, int pos);
int sbArrayPop(SBArray *array, void **ptr);
int sbArrayPush(SBArray *array, void *ptr);
int sbArrayRemove(SBArray *array, int pos);
int sbArraySlice(const SBArray *array, int start, int end, SBArray *newArray);

#endif /* SBARRAY_H */
