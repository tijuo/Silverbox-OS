#include "sbassocarray.h"
#include <stdlib.h>
#include <string.h>

static unsigned long _hash_func( unsigned char *key, size_t keysize )
{
  unsigned long hash = 0;

  for( size_t i=0; i < keysize; i++ )
  {
    hash += key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}

int sbAssocArrayCopy( const SBAssocArray *array, SBAssocArray *copy )
{
  if( !array || !copy )
    return SBAssocArrayError;

  if( sbAssocArrayCreate( copy, array->numBuckets ) != 0 )
    return SBAssocArrayFailed;

  memcpy( copy->buckets, array->buckets, 
          array->numBuckets * sizeof( struct _KeyValPair ) );

  return 0;
}

int sbAssocArrayCreate( SBAssocArray *array, size_t numBuckets )
{
  if( !array )
    return SBAssocArrayError;

  array->buckets = calloc( numBuckets, sizeof( struct _KeyValPair ) );

  if( !array->buckets )
  {
    return SBAssocArrayFailed;
    array->numBuckets = 0;
  }

  array->numBuckets = numBuckets;

  return 0;
}

int sbAssocArrayDelete( SBAssocArray *array )
{
  if( !array )
    return SBAssocArrayError;

  if( array->buckets )
  {
    free(array->buckets);
    array->buckets = NULL;
    array->numBuckets = 0;
  }

  return 0;
}

/* XXX: Warning: This assumes that the key will stay in memory. It does not
   copy the key! */

int sbAssocArrayInsert( SBAssocArray *array, void *key, size_t keysize, void *value )
{
  struct _KeyValPair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( !array || !key || !array->buckets || array->numBuckets == 0 )
    return SBAssocArrayError;

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &array->buckets[(hash_index + (i*i)) % array->numBuckets];
    i++;

    if( !pair->valid )
    {
      pair->pair.key = key;
      pair->pair.keysize = keysize;
      pair->pair.value = value;
      pair->valid = 1;
      break;
    }

    if( i * i >= array->numBuckets )
      return SBAssocArrayFull;
  }

  return 0;
}

int sbAssocArrayLookup( const SBAssocArray *array, void *key, size_t keysize, void **val )
{
  struct _KeyValPair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( array == NULL || key == NULL || array->buckets == NULL ||
      array->numBuckets == 0 )
  {
    return SBAssocArrayError;
  }

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &array->buckets[(hash_index + (i*i)) % array->numBuckets];
    i++;

    if( pair->valid && pair->pair.keysize == keysize && memcmp(key, pair->pair.key, keysize) == 0 )
    {
      if( pair->pair.value )
        *(int **)val = (int *)pair->pair.value;

      return 0;
    }
    if( i * i >= array->numBuckets )
      return SBAssocArrayNotFound;
  }

  return SBAssocArrayNotFound;
}

int sbAssocArrayMerge( const SBAssocArray *array1, const SBAssocArray *array2,
                       SBAssocArray *newArray )
{
  if( !array1 || !array2 || !newArray )
    return SBAssocArrayError;

  if( sbAssocArrayCreate( newArray, 2 * (array1->numBuckets + 
        array2->numBuckets) ) != 0 )
  {
    return SBAssocArrayFailed;
  }

  for( unsigned i=0; i < array1->numBuckets; i++ )
  {
    if( array1->buckets[i].valid )
    {
      sbAssocArrayInsert(newArray, array1->buckets[i].pair.key,
         array1->buckets[i].pair.keysize, array1->buckets[i].pair.value);
    }
  }

  for( unsigned i=0; i < array2->numBuckets; i++ )
  {
    if( array2->buckets[i].valid )
    {
      sbAssocArrayInsert(newArray, array2->buckets[i].pair.key,
         array2->buckets[i].pair.keysize, array2->buckets[i].pair.value);
    }
  }

  return 0;
}

int sbAssocArrayRemove( SBAssocArray *array, void *key, size_t keysize )
{
  struct _KeyValPair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( array == NULL || key == NULL || array->buckets == NULL ||
      array->numBuckets == 0 )
  {
    return SBAssocArrayError;
  }

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &array->buckets[(hash_index + (i*i)) % array->numBuckets];
    i++;

    if( pair->valid && pair->pair.keysize == keysize && memcmp(key, pair->pair.key, keysize) == 0 )
    {
      pair->valid = 0;
      return 0;
    }

    if( i * i >= array->numBuckets )
      return SBAssocArrayNotFound;
  }

  return SBAssocArrayNotFound;
}

