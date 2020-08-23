#ifndef SB_HASH_H
#define SB_HASH_H

#include <stdlib.h>
#include <os/ostypes/ostypes.h>

struct KeyValuePair
{
  const char *key;
  const void *value;
  int valid;
};

typedef struct SBHash
{
  struct KeyValuePair *buckets;
  size_t numBuckets;
} sbhash_t;

sbhash_t *sbHashCopy(const sbhash_t *array, sbhash_t *copy);
sbhash_t *sbHashCreate(sbhash_t *array, size_t numBuckets);
void sbHashDestroy(sbhash_t *array);
int sbHashInsert(sbhash_t *array, const char *key, const void *value);
size_t sbHashKeys(const sbhash_t *array, const char **keys);
int sbHashLookup(const sbhash_t *array, const void *key, const void **val);
int sbHashMerge(sbhash_t *array1, const sbhash_t *array2);
int sbHashRemove(sbhash_t *array, const void *key);

#endif /* SB_ASSOC_ARRAY_H */
