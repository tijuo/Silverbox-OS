#ifndef SB_ASSOC_ARRAY_H
#define SB_ASSOC_ARARY_H

#include <stdlib.h>

struct _SBPair
{
  void *key;
  size_t keysize;
  void *value;
  size_t valsize;
};

struct _SBKey
{
  void *key;
  size_t keysize;
};

struct _SBValue
{
  void *value;
  size_t valsize;
};

typedef struct _SBPair SBPair;
typedef struct _SBKey SBKey;
typedef struct _SBValue SBValue;

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

int sbAssocArrayCopy( const SBAssocArray *array, SBAssocArray *copy );
int sbAssocArrayCreate( SBAssocArray *array, size_t numBuckets );
int sbAssocArrayDelete( SBAssocArray *array );
int sbAssocArrayInsert( SBAssocArray *array, void *key, size_t keysize, 
                        void *value, size_t valsize );
int sbAssocArrayKeys( const SBAssocArray *array, SBKey **keys, size_t *numKeys );
int sbAssocArrayLookup( const SBAssocArray *array, void *key, size_t keysize, 
                        void **val, size_t *valsize );
int sbAssocArrayMerge( const SBAssocArray *array1, const SBAssocArray *array2,
                       SBAssocArray *newArray );
int sbAssocArrayRemove( SBAssocArray *array, void *key, size_t keysize,
                        void **value, size_t *valsize );

#endif /* SB_ASSOC_ARRAY_H */
