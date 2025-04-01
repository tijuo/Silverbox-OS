#ifndef OS_HASH_TABLE_H
#define OS_HASH_TABLE_H

#include <stdlib.h>
#include <os/ostypes/ostypes.h>
#include <os/allocator.h>
#include <os/ostypes/string.h>

struct StringKVPair;
struct IntKVPair;

typedef struct StringKVPair {
    char *key;
    void* value;
    size_t value_size;
    struct StringKVPair* next;
} StringKVPair;

typedef SLICE(StringKVPair) StringKVPairSlice;

typedef struct {
    StringKVPairSlice buckets;
    Allocator allocator;
} StringHashTable;

typedef struct IntKVPair {
    int key;
    void* value;
    size_t value_size;
    struct IntKVPair* next;
} IntKVPair;

typedef SLICE(IntKVPair) IntKVPairSlice;

typedef struct {
    IntKVPairSlice buckets;
    Allocator allocator;
} IntHashTable;

StringHashTable* string_hash_table_init(StringHashTable* htable, size_t num_buckets, Allocator *allocator);
StringHashTable* string_hash_table_copy(const StringHashTable* htable, StringHashTable* out_copy);
void string_hash_table_destroy(StringHashTable* htable);
int string_hash_table_insert(StringHashTable* htable, StrSlice key, void* value, size_t val_size);
size_t string_hash_table_keys(const StringHashTable* htable, StrSlice (*keys)[], size_t max_keys);
int string_hash_table_lookup(const StringHashTable* htable, StrSlice key, void** val, size_t *val_size);
int string_hash_table_lookup_pair(const StringHashTable* htable, StrSlice key, StrSlice* stored_key, void** val, size_t **val_size);
//int string_hash_table_merge(StringHashTable* htable1, const StringHashTable* htable2);
int string_hash_table_remove(StringHashTable* htable, StrSlice key);
int string_hash_table_remove_pair(StringHashTable* htable, StrSlice key, void** value, size_t *val_size);

IntHashTable* int_hash_table_init(IntHashTable* htable, size_t num_buckets, Allocator *allocator);
IntHashTable* int_hash_table_copy(const IntHashTable* htable, IntHashTable* out_copy);
void int_hash_table_destroy(IntHashTable* htable);
int int_hash_table_insert(IntHashTable* htable, int key, void* value, size_t val_size);
size_t int_hash_table_keys(const IntHashTable* htable, int (*keys)[], size_t max_keys);
int int_hash_table_lookup(const IntHashTable* htable, int key, void** val, size_t *val_size);
int int_hash_table_lookup_pair(const IntHashTable* htable, int key, int** stored_key, void** val, size_t **val_size);
int int_hash_table_merge(IntHashTable* htable1, const IntHashTable* htable2);
int int_hash_table_remove(IntHashTable* htable, int key);
int int_hash_table_remove_value(IntHashTable* htable, int key, void** value, size_t *val_size);

#endif /* OS_HASH_TABLE_H */
