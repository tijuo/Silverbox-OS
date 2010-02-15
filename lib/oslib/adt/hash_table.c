#include <os/adt/hash_table.h>
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

int sbAssocArrayCreate( SBAssocArray *array, size_t numBuckets )
{
  if( !array )
    return SBAssocArrayError;

  array->buckets = calloc( numBuckets, sizeof( struct _KeyValPair ) );
  array->numBuckets = numBuckets;
}

int sbAssocArrayInsert( SBAssocArray *array, void *key, size_t keysize, void *value )
{
  struct _KeyValPair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( !array || !key || !table->buckets || table->numBuckets == 0 )
    return SBAssocArrayError;

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &table->buckets[(hash_index + (i*i)) % table->numBuckets];
    i++;

    if( !pair->valid )
    {
      pair->pair.key = key;
      pair->pair.keysize = keysize;
      pair->pair.val = value;
      pair->valid = 1;
      break;
    }

    if( i * i >= table->numBuckets )
      return SBAssocArrayFull;
  }

  return 0;
}

int sbAssocArrayLookup( SBAssocArray *array, void *key, size_t keysize, void **val )
{
  struct _KeyValPair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( table == NULL || key == NULL || table->buckets == NULL ||
      table->numBuckets == 0 )
  {
    return SBAssocArrayError;
  }

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &table->buckets[(hash_index + (i*i)) % table->numBuckets];
    i++;

    if( pair->valid && pair->pair.keysize == keysize && memcmp(key, pair->pair.key, keysize) )
    {
      if( pair->pair.val )
        *val = pair->pair.val;

      return 0;
    }
    if( i * i >= table->numBuckets )
      return SBAssocArrayNotFound;
  }

  return SBAssocArrayNotFound;
}

int sbAssocArrayRemove( SBAssocArray *array, void *key, size_t keysize )
{
  struct _KeyValPair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( table == NULL || key == NULL || table->buckets == NULL ||
      table->numBuckets == 0 )
  {
    return SBAssocArrayError;
  }

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &table->buckets[(hash_index + (i*i)) % table->numBuckets];
    i++;

    if( pair->valid && pair->pair.keysize == keysize && memcmp(key, pair->pair.key, keysize) )
    {
      pair->valid = 0;
      return 0;
    }

    if( i * i >= table->numBuckets )
      return SBArrayNotFound;
  }

  return SBArrayNotFound;
}

