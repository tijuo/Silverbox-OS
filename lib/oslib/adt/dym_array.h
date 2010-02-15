#ifndef DYM_ARRAY_H
#define DYM_ARRAY_H

enum SBArrayValues { SBArrayNotFound=-1, SBArrayFailed=-2, SBArrayEmpty=-3 };

struct _SBArray
{
  void **ptrs;
  int nElems;
  int capacity;

  int (*clear)(struct _SBArray *);
  int (*elemAt)(struct _SBArray *, int, void *);
  int (*find)(struct _SBArray, void *elem);
  int (*insert)(struct _SBArray *, void *, int);
  int (*length)(struct _SBArray *);
  int (*pop)(struct _SBArray *, void *);
  int (*push)(struct _SBArray *, void *);
  int (*remove)(struct _SBArray *, int);
};

typedef struct _SBArray SBArray;

SBArray *sbArrayCreate(int numElems, ...);
int sbArrayElemAt(SBArray *array, int pos, void *elem);
int sbArrayFind(SBArray *array, void *elem);
int sbArrayInsert(SBArray *array, void *ptr, int pos);
int sbArrayLength(SBArray *array);
int sbArrayPop(SBArray *array, void **ptr);
int sbArrayPush(SBArray *arrary, void *ptr);
int sbArrayRemove(SBArray *array, int pos);

#endif /* DYM_ARRAY_H */
