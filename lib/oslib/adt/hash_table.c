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

HashTable hashCreate( size_t numBuckets )
{
  HashTable table = { NULL, 0 };

  table.buckets = calloc( numBuckets, sizeof( struct _KV_Pair ) );
  table.numBuckets = numBuckets;
}

int hashInsert( void *key, size_t keysize, void *value, HashTable *table )
{
  struct _KV_Pair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( table == NULL || key == NULL || table->buckets == NULL || 
      table->numBuckets == 0 )
  {
    return -1;
  }

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &table->buckets[(hash_index + (i*i)) % table->numBuckets];
    i++;

    if( !pair->valid )
    {
      pair->key = key;
      pair->keysize = keysize;
      pair->val = value;
      pair->valid = 1;
      break;
    }

    if( i * i >= table->numBuckets )
      return -1;
  }

  return 0;
}

int hashLookup( void *key, size_t keysize, void **val, HashTable *table )
{
  struct _KV_Pair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( table == NULL || key == NULL || table->buckets == NULL ||
      table->numBuckets == 0 )
  {
    return -1;
  }

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &table->buckets[(hash_index + (i*i)) % table->numBuckets];
    i++;

    if( pair->valid && pair->keysize == keysize && memcmp(key, pair->key, keysize) )
    {
      if( pair->val )
        *val = pair->val;

      return 0;
    }
    if( i * i >= table->numBuckets )
      return -1;
  }

  return -1;
}

int hashRemove( void *key, size_t keysize, HashTable *table )
{
  struct _KV_Pair *pair;
  unsigned i = 0;
  size_t hash_index;

  if( table == NULL || key == NULL || table->buckets == NULL ||
      table->numBuckets == 0 )
  {
    return -1;
  }

  hash_index = _hash_func(key,keysize);

  while( 1 )
  {
    pair = &table->buckets[(hash_index + (i*i)) % table->numBuckets];
    i++;

    if( pair->valid && pair->keysize == keysize && memcmp(key, pair->key, keysize) )
    {
      pair->valid = 0;
      return 0;
    }

    if( i * i >= table->numBuckets )
      return -1;
  }

  return -1;
}

