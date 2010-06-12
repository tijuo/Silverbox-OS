#include <os/ostypes/sbassocarray.h>
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
  struct _KeyValPair *pair, *copyPair;

  if( !array || !copy )
    return SBAssocArrayError;

  if( sbAssocArrayCreate( copy, array->numBuckets ) != 0 )
    return SBAssocArrayFailed;

  pair = array->buckets;

  for( unsigned i=0; i < array->numBuckets; i++, pair++ )
  {
    if( pair->valid )
    {
      copyPair = &copy->buckets[i];

      copyPair->pair.value = pair->pair.value;
      copyPair->pair.valsize = pair->pair.valsize;
      copyPair->pair.keysize = pair->pair.keysize;
      copyPair->pair.key = malloc(pair->pair.keysize);

      memcpy(copyPair->pair.key, pair->pair.key, pair->pair.keysize);

      if( !copyPair->pair.key )
      {
        sbAssocArrayDelete( copy );
        return SBAssocArrayFailed;
      }

      copyPair->valid = 1;
    }
    
  }

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

/* XXX: Warning: Keys and Values are not freed */

int sbAssocArrayDelete( SBAssocArray *array )
{
  if( !array )
    return SBAssocArrayError;

  if( array->buckets )
  {
    free(array->buckets);
    array->buckets = NULL;
  }

  array->numBuckets = 0;

  return 0;
}

/* XXX: Warning: This assumes that the key will stay in memory. It does not
   copy the key! */

int sbAssocArrayInsert( SBAssocArray *array, void *key, size_t keysize, 
                        void *value, size_t valsize )
{
  struct _KeyValPair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( !array || !key || !array->buckets || array->numBuckets == 0 || keysize == 0 )
    return SBAssocArrayError;
  else if( value == NULL && valsize != 0 )
    return SBAssocArrayError;

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &array->buckets[(hash_index + (i*i)) % array->numBuckets];

    if( !pair->valid )
    {
      pair->pair.key = malloc(keysize);

      if( !pair->pair.key )
        return SBAssocArrayFailed;

      memcpy(pair->pair.key, key, keysize);

      pair->pair.value = value;

      pair->pair.keysize = keysize;
      pair->pair.valsize = valsize;
      pair->valid = 1;
      break;
    }

    if( i * i >= array->numBuckets )
      return SBAssocArrayFull;

    i++;
  }

  return 0;
}

int sbAssocArrayKeys( const SBAssocArray *array, SBKey **keys, size_t *numKeys )
{
  size_t keyCount=0;
  struct _KeyValPair *kvp;

  if( !array )
    return SBAssocArrayError;

  if( keys )
    *keys = NULL;

  if( !array->buckets )
  {
    if( numKeys )
      numKeys = 0;

    return SBAssocArrayFailed;
  }

  kvp = (struct _KeyValPair *)&array->buckets;

  for( unsigned i=0; i < array->numBuckets; i++ )
  {
    if( kvp->valid )
    {
      keyCount++;

      if( keys )
      {
        *keys = realloc(*keys, keyCount * sizeof(SBKey));

        if( !*keys )
        {
          *numKeys = 0;

          return SBAssocArrayFailed;
        }

        keys[i]->key = kvp->pair.key;
        keys[i]->keysize = kvp->pair.keysize;
      }
    }
  }
  return 0;
}

int sbAssocArrayLookup( const SBAssocArray *array, void *key, size_t keysize, 
                        void **val, size_t *valsize )
{
  struct _KeyValPair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( array == NULL || key == NULL )
    return SBAssocArrayError;
  else if( array->buckets == NULL || array->numBuckets == 0 )
    return SBAssocArrayNotFound;

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &array->buckets[(hash_index + (i*i)) % array->numBuckets];

    if( pair->valid && pair->pair.keysize == keysize && 
          memcmp(key, pair->pair.key, keysize) == 0 )
    {
      if( val )
        *(int **)val = (int *)pair->pair.value;

      if( valsize )
        *valsize = pair->pair.valsize;
      return 0;
    }

    if( i * i >= array->numBuckets )
      return SBAssocArrayNotFound;

    i++;
  }

  return SBAssocArrayNotFound;
}

/* Since these associative arrays are of fixed size, two arrays can be
   merged together to form a larger one. */

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
         array1->buckets[i].pair.keysize, array1->buckets[i].pair.value,
         array1->buckets[i].pair.valsize);
    }
  }

  for( unsigned i=0; i < array2->numBuckets; i++ )
  {
    if( array2->buckets[i].valid )
    {
      sbAssocArrayInsert(newArray, array2->buckets[i].pair.key,
         array2->buckets[i].pair.keysize, array2->buckets[i].pair.value,
         array2->buckets[i].pair.valsize);
    }
  }

  return 0;
}

int sbAssocArrayRemove( SBAssocArray *array, void *key, size_t keysize,
   void **value, size_t *valsize )
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

    if( pair->valid && pair->pair.keysize == keysize && 
          memcmp(key, pair->pair.key, keysize) == 0 )
    {
      if( value )
        *(int **)value = (int *)pair->pair.value;

      if( valsize )
        *valsize = pair->pair.valsize;

      free(pair->pair.key);

      pair->valid = 0;
      return 0;
    }

    if( i * i >= array->numBuckets )
      return SBAssocArrayNotFound;

    i++;
  }

  return SBAssocArrayNotFound;
}
