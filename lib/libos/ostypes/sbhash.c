#include <os/ostypes/sbhash.h>
#include <stdlib.h>
#include <string.h>

static unsigned long _hash_func(char *key, size_t keysize) {
  unsigned long hash = 0;

  for(size_t i = 0; i < keysize; i++) {
    hash += (unsigned int)key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}

sbhash_t* sbHashCopy(sbhash_t *htable, sbhash_t *copy) {
  struct KeyValuePair *pair, *copyPair;

  if(!htable || !copy)
    return NULL;

  if(sbHashCreate(copy, htable->numBuckets) != 0)
    return NULL;

  for(size_t i = 0; i < htable->numBuckets; i++) {
    pair = &htable->buckets[i];
    copyPair = &copy->buckets[i];

    if(pair->valid)
      *copyPair = *pair;
  }

  return copy;
}

sbhash_t* sbHashCreate(sbhash_t *htable, size_t numBuckets) {
  if(!htable)
    return NULL;

  htable->buckets = calloc(numBuckets, sizeof(struct KeyValuePair));

  if(!htable->buckets)
    return NULL;

  htable->numBuckets = numBuckets;

  return htable;
}

void sbHashDestroy(sbhash_t *htable) {
  if(!htable)
    return;

  if(htable->buckets) {
    free(htable->buckets);
    htable->buckets = NULL;
  }

  htable->numBuckets = 0;

  return;
}

int sbHashInsert(sbhash_t *htable, char *key, void *value) {
  struct KeyValuePair *pair;
  size_t hashIndex;

  if(!htable || !htable->buckets || htable->numBuckets == 0)
    return SB_FAIL;

  hashIndex = _hash_func(key, strlen(key));

  for(size_t i = 0; i < htable->numBuckets; i++) {
    pair = &htable->buckets[(hashIndex + i) % htable->numBuckets];

    if(!pair->valid) {
      pair->key = key;
      pair->value = value;
      pair->valid = 1;
      return 0;
    }
  }

  return SB_FULL;
}

size_t sbHashKeys(sbhash_t *htable, char **keys) {
  size_t keyCount = 0;
  struct KeyValuePair *pair;

  if(htable && htable->buckets) {
    for(size_t i = 0; i < htable->numBuckets; i++) {
      pair = &htable->buckets[i];

      if(pair->valid) {
        keyCount++;

        if(keys)
          *(keys++) = pair->key;
      }
    }
  }

  return keyCount;
}

int sbHashLookup(sbhash_t *htable, char *key, void **val) {
  return sbHashLookupPair(htable, key, NULL, val);
}

int sbHashLookupPair(sbhash_t *htable, char *key, char **storedKey, void **val)
{
  struct KeyValuePair *pair;
  size_t hashIndex;

  if(!htable || !htable->buckets)
    return SB_FAIL;

  hashIndex = _hash_func(key, strlen(key));

  for(size_t i = 0; i < htable->numBuckets; i++) {
    pair = &htable->buckets[(hashIndex + i) % htable->numBuckets];

    if(pair->valid && strcmp(pair->key, key) == 0) {
      if(val)
        *(void**)val = pair->value;

      if(storedKey)
        *storedKey = pair->key;

      return 0;
    }
  }

  return SB_NOT_FOUND;
}

/* Since these hash tables are of fixed size, two arrays can be
 merged together to form a larger one. */

int sbHashMerge(sbhash_t *htable1, sbhash_t *htable2) {
  if(!htable1)
    return SB_FAIL;

  if(htable2) {
    for(size_t i = 0; i < htable2->numBuckets; i++) {
      struct KeyValuePair *pair = &htable2->buckets[i];

      if(pair->valid) {
        void *elem;

        if(sbHashLookup(htable1, pair->key, &elem) != 0) {
          if(sbHashInsert(htable1, pair->key, pair->value) != 0)
            return SB_FAIL;
        }
        else if(elem != pair->value)
          return SB_CONFLICT;
      }
    }
  }

  return 0;
}

int sbHashRemove(sbhash_t *htable, char *key) {
  return sbHashRemovePair(htable, key, NULL, NULL);
}

int sbHashRemovePair(sbhash_t *htable, char *key, char **storedKey, void **elem)
{
  struct KeyValuePair *pair;
  size_t hashIndex;

  if(!htable || !htable->buckets)
    return SB_FAIL;

  hashIndex = _hash_func(key, strlen(key));

  for(size_t i = 0; i < htable->numBuckets; i++) {
    pair = &htable->buckets[(hashIndex + i) % htable->numBuckets];

    if(pair->valid && strcmp(pair->key, key) == 0) {
      if(storedKey)
        *storedKey = pair->key;

      if(elem)
        *elem = pair->value;

      pair->valid = 0;
      return 0;
    }
  }

  return SB_NOT_FOUND;
}
