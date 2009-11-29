#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <types.h>

struct _KV_Pair
{
  void *key;
  size_t keysize;
  void *val;
  int valid;
};

struct _HashTable
{
  struct _KV_Pair *buckets;
  size_t numBuckets;
};

typedef struct _HashTable HashTable;

HashTable hashCreate( size_t numBuckets );
int hashInsert( void *key, size_t keysize, void *value, HashTable *table );
int hashLookup( void *key, size_t keysize, void **val, HashTable *table );
int hashRemove( void *key, size_t keysize, HashTable *table );

#endif /* HASH_TABLE_H */
