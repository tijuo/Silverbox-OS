#ifndef SB_HASH_H
#define SB_HASH_H

#include <stdlib.h>
#include <os/ostypes/ostypes.h>

struct KeyValuePair
{
  void *key;
  void *value;
  int valid;
};

typedef struct SBHash
{
  struct KeyValuePair *buckets;
  size_t numBuckets;
} sbhash_t;

sbhash_t *sbHashCopy(sbhash_t *array, sbhash_t *copy);
sbhash_t *sbHashCreate(sbhash_t *array, size_t numBuckets);
void sbHashDestroy(sbhash_t *array);
int sbHashInsert(sbhash_t *array, void *key, void *value);
size_t sbHashKeys(sbhash_t *array, void **keys);
int sbHashLookup(sbhash_t *array, void *key, void **val);
int sbHashMerge(sbhash_t *array1, sbhash_t *array2);
int sbHashRemove(sbhash_t *array, void *key);

#endif /* SB_ASSOC_ARRAY_H */
