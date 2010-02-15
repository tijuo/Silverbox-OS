#ifndef SB_ASSOC_ARRAY_H
#define SB_ASSOC_ARARY_H

#include <types.h>

struct _SBPair
{
  void *key;
  size_t keysize;
  void *value;
};

typedef struct _SBPair SBPair;

struct _KeyValPair
{
  SBPair pair;
  int valid;
};

struct _SBAssocArray
{
  struct _KeyValPair *buckets;
  size_t numBuckets;
};

typedef struct _SBAssocArray SBAssocArray;

enum SBAssocArrayStatus { SBAssocArrayError=-1, SBAssocArrayFailed=-2, 
                          SBAssocArrayNotFound=-3, SBAssocArrayFull=-4 };

int sbAssocArrayCreate( SBAssocArray *array, size_t numBuckets );
int sbAssocArrayInsert( SBAssocArray *array, void *key, size_t keysize, 
                        void *value );
int sbAssocArrayLookup( SBAssocArray *array, void *key, size_t keysize, 
                        void **val );
int sbAssocArrayRemove( SBAssocArray *array, void *key, size_t keysize );

#endif /* SB_ASSOC_ARRAY_H */
