#include <os/ostypes/hashtable.h>
#include <os/ostypes/slice.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BUCKETS 16

static unsigned long hash_func(ByteSlice slice)
{
    unsigned long hash = 0;

    for(size_t i = 0; i < slice.length; i++) {
        hash += (unsigned long)slice.ptr[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

/**
 * Create a shallow copy of a string hash table. The keys will be cloned using the cloner's allocator. The elements will be
 * shallow copied without allocation.
 * 
 * @param htable The table to be copied.
 * @param out_copy The copied table.
 * @return The copied table, `out_copy`.
 */
StringHashTable* string_hash_table_copy(const StringHashTable* htable, StringHashTable* out_copy)
{
    if(!htable->buckets.ptr) {
        return NULL;
    }

    if(!string_hash_table_init(out_copy, htable->buckets.length, &htable->allocator)) {
        return NULL;
    }

    StringKVPair* pair = &htable->buckets.ptr[0];
    StringKVPair* copy_pair = &out_copy->buckets.ptr[0];
    StringKVPair *end = &htable->buckets.ptr[htable->buckets.length];
    StringKVPair *ptr;
    StringKVPair *copy_ptr;

    // For each chain
    for(; pair != end; pair++, copy_pair++) {
        if(!pair->key) {
            memset(copy_pair, 0, sizeof *copy_pair);
            continue;
        }

        // Copy the chain linked-list
        for(ptr=pair, copy_ptr=copy_pair; ptr && copy_ptr; ptr=ptr->next, copy_ptr=copy_ptr->next) {
            size_t key_len = strlen(ptr->key);

            copy_ptr->key = malloc(key_len + 1);

            if(!copy_ptr->key) {
                string_hash_table_destroy(out_copy);
                return NULL;
            }

            strcpy(copy_ptr->key, ptr->key);

            copy_ptr->value = ptr->value;
            copy_ptr->value_size = ptr->value_size;

            if(copy_ptr != copy_pair && ptr->next) {
                ByteSlice slice = allocator_alloc_zeroed(&htable->allocator, sizeof(StringKVPair), sizeof(long));

                if(!slice.ptr) {
                    free(copy_ptr->key);
                    string_hash_table_destroy(out_copy);
                    return NULL;
                }

                copy_ptr->next = (StringKVPair *)slice.ptr;
            }
        }
    }

    return out_copy;
}

/**
 * Initialize an uninitialized string hash table.
 * 
 * @param htable - The hash table to be initialized. The table must not have been previously initialized.
 * @param num_buckets - The number of buckets to be used. If `0`, then initialize with the default number.
 * @param allocator - The allocator to be used for the hash table. All allocations (keys and bucket entries) will use this allocator.
 * If `NULL`, then use the global allocator.
 * @return The initialized hash table, `htable`. `NULL`, on error.
 */
StringHashTable* string_hash_table_init(StringHashTable* htable, size_t num_buckets, Allocator *allocator)
{
    htable->allocator = allocator ? *allocator : NULL_ALLOCATOR;
    num_buckets = num_buckets ? num_buckets : DEFAULT_BUCKETS;

    ByteSlice new_slice = allocator_alloc_zeroed(&htable->allocator, num_buckets * sizeof(StringKVPair), 4);

    if(!new_slice.ptr) {
        return NULL;
    }

    htable->buckets = CAST_SLICE(StringKVPairSlice, new_slice);

    return htable;
}

void string_hash_table_destroy(StringHashTable* htable)
{
    if(!htable->buckets.ptr) {
        return;
    }

    StringKVPair *pair = &htable->buckets.ptr[0];
    StringKVPair *end = &htable->buckets.ptr[htable->buckets.length];

    for(; pair != &htable->buckets.ptr[0] && pair != end; pair++) {
        if(pair->key) {
            free(pair->key);

            for(StringKVPair *chain_ptr=pair->next; chain_ptr; ) {
                StringKVPair *next = chain_ptr->next;

                free(chain_ptr->key);
                allocator_free(&htable->allocator, chain_ptr);
                
                chain_ptr = next;
            }
        }

        pair->next = NULL;
    }

    allocator_free(&htable->allocator, &htable->buckets.ptr[0]);

    htable->buckets.ptr = NULL;
    htable->buckets.length = 0;
}

int string_hash_table_insert(StringHashTable* htable, StrSlice key, void* value, size_t value_size)
{
    unsigned long hash_index = hash_func(CAST_SLICE(ByteSlice, key)) % htable->buckets.length;
    StringKVPair *pair = &htable->buckets.ptr[hash_index];
    StringKVPair *new_pair;
    StringKVPair *next_pair;
    char *new_key = malloc(key.length + 1);
    
    if(!new_key) {
        return SB_FAIL;
    }

    strcpy(new_key, key.ptr);

    if(!pair->key) {
        new_pair = pair;
        next_pair = NULL;
    } else {
        ByteSlice slice = allocator_alloc_zeroed(&htable->allocator, sizeof(StringKVPair), sizeof(int));

        if(!slice.ptr) {
            free(new_key);
            return SB_FAIL;
        }

        next_pair = pair->next;
        new_pair = (StringKVPair *)slice.ptr;
        pair->next = new_pair;
    }

    new_pair->key = new_key;
    new_pair->next = next_pair;
    new_pair->value = value;
    new_pair->value_size = value_size;
    
    return 0;
}

size_t string_hash_table_keys(const StringHashTable* htable, StrSlice (*keys)[], size_t max_keys)
{
    size_t key_count = 0;
    StringKVPair* pair = &htable->buckets.ptr[0];
    StringKVPair* end  = &htable->buckets.ptr[htable->buckets.length];

    for(; pair != end && key_count < max_keys; pair++) {
        if(!pair->key) {
            continue;
        }

        for(StringKVPair *ptr=pair; ptr && key_count < max_keys; ptr = pair->next) {
            (*keys)[key_count++] = (StrSlice) { .ptr = ptr->key, .length = strlen(ptr->key) };
        }
    }

    return key_count;
}

int string_hash_table_lookup(const StringHashTable* htable, StrSlice key, void** val, size_t *val_size)
{
    size_t *s;
    int retval = string_hash_table_lookup_pair(htable, key, NULL, val, &s);

    if(val_size && s) {
        *val_size = *s;
    }

    return retval;
}

int string_hash_table_lookup_pair(const StringHashTable* htable, StrSlice key, StrSlice *stored_key, void** val, size_t **val_size)
{
    unsigned long hash_index = hash_func(CAST_SLICE(ByteSlice, key));

    for(StringKVPair* pair=&htable->buckets.ptr[hash_index]; pair; pair=pair->next) {
        if(pair->key && strcmp(key.ptr, pair->key) == 0) {
            if(stored_key) {
                stored_key->ptr = pair->key;
                stored_key->length = strlen(pair->key);
            }

            if(val) {
                *val = pair->value;
            }

            if(val_size) {
                *val_size = &pair->value_size;
            }
            
            return 0;
        }
    }

    return SB_NOT_FOUND;
}

/* Since these hash tables are of fixed size, two htables can be
 merged together to form a larger one. */
/*
int string_hash_table_merge(StringHashTable* htable1, StringHashTable* htable2)
{
    if(!htable1)
        return SB_FAIL;

    if(htable2) {
        for(size_t i = 0; i < htable2->num_buckets; i++) {
            StringKVPair* pair = &htable2->buckets[i];

            if(pair->valid) {
                void* elem;

                if(string_hash_table_lookup(htable1, pair->key, &elem) != 0) {
                    if(string_hash_table_insert(htable1, pair->key, pair->value) != 0)
                        return SB_FAIL;
                } else if(elem != pair->value)
                    return SB_CONFLICT;
            }
        }
    }

    return 0;
}
*/

int string_hash_table_remove(StringHashTable* htable, StrSlice key)
{
    return string_hash_table_remove_pair(htable, key, NULL, NULL);
}

int string_hash_table_remove_pair(StringHashTable* htable, StrSlice key, void** value, size_t *val_size)
{
    unsigned long hash_index = hash_func(CAST_SLICE(ByteSlice, key));
    StringKVPair *prev = NULL;

    for(StringKVPair* pair=&htable->buckets.ptr[hash_index]; pair; prev=pair, pair=pair->next) {
        if(pair->key && strcmp(key.ptr, pair->key) == 0) {
            if(value) {
                *value = pair->value;
            }
            
            if(val_size) {
                *val_size = pair->value_size;
            }

            pair->key = NULL;
            pair->value = NULL;

            if(!prev) {
                if(pair->next) {
                    *pair = *pair->next;
                } else {
                    memset(pair, 0, sizeof *pair);
                }
            } else {
                prev->next = pair->next;

                free(pair->key);
                allocator_free(&htable->allocator, pair);
            }

            return 0;
        }
    }

    return SB_NOT_FOUND;
}

IntHashTable* int_hash_table_init(IntHashTable* htable, size_t num_buckets, Allocator *allocator) {
    htable->allocator = allocator ? *allocator : NULL_ALLOCATOR;
    num_buckets = num_buckets ? num_buckets : DEFAULT_BUCKETS;

    ByteSlice new_slice = allocator_alloc_zeroed(&htable->allocator, num_buckets * sizeof(IntKVPair), 4);

    if(!new_slice.ptr) {
        return NULL;
    }

    htable->buckets = CAST_SLICE(IntKVPairSlice, new_slice);

    return htable;
}

IntHashTable* int_hash_table_copy(const IntHashTable* htable, IntHashTable* out_copy) {
    if(!int_hash_table_init(out_copy, htable->buckets.length, &htable->allocator)) {
        return NULL;
    }

    IntKVPair* pair = &htable->buckets.ptr[0];
    IntKVPair* copy_pair = &out_copy->buckets.ptr[0];
    IntKVPair *end = &htable->buckets.ptr[htable->buckets.length];
    IntKVPair *ptr;
    IntKVPair *copy_ptr;

    for(; pair != end; pair++, copy_pair++) {
        if(!pair->value) {
            memset(copy_pair, 0, sizeof *copy_pair);
            continue;
        }

        for(ptr=pair, copy_ptr=copy_pair; ptr && copy_ptr; ptr=ptr->next, copy_ptr=copy_ptr->next) {
            copy_ptr->key = ptr->key;
            copy_ptr->value = ptr->value;

            if(copy_ptr != copy_pair && ptr->next) {
                ByteSlice slice = allocator_alloc_zeroed(&htable->allocator, sizeof(IntKVPair), sizeof(long));

                if(!slice.ptr) {
                    int_hash_table_destroy(out_copy);
                    return NULL;
                }

                copy_ptr->next = (IntKVPair *)slice.ptr;
            }
        }
    }
    return out_copy;
}

void int_hash_table_destroy(IntHashTable* htable) {
    IntKVPair* pair = &htable->buckets.ptr[0];
    IntKVPair *ptr;
    IntKVPair *next;
    IntKVPair *end = &htable->buckets.ptr[htable->buckets.length];

    for(; pair != end; pair++) {
        if(pair->value) {
            for(ptr=pair->next; ptr; ptr=next) {
                next = ptr->next;
                free(ptr);
            }
        }

        pair->next = NULL;
    }
}

int int_hash_table_insert(IntHashTable* htable, int key, void* value, size_t value_size) {
    if(!value) {
        return SB_FAIL; // inserting NULL values for elements is not allowed
    }

    unsigned long hash_index = hash_func((SLICE_FROM_ARRAY(ByteSlice, (unsigned char *)&key, sizeof(int)))) % htable->buckets.length;
    IntKVPair *pair = &htable->buckets.ptr[hash_index];
    IntKVPair *new_pair;
    IntKVPair *next_pair;

    if(!pair->value) {
        new_pair = pair;
        next_pair = NULL;
    } else {
        ByteSlice slice = allocator_alloc_zeroed(&htable->allocator, sizeof(StringKVPair), sizeof(int));

        if(!slice.ptr) {
            return SB_FAIL;
        }

        next_pair = pair->next;
        new_pair = (IntKVPair *)slice.ptr;
        pair->next = new_pair;
    }

    new_pair->key = key;
    new_pair->next = next_pair;
    new_pair->value = value;
    new_pair->value_size = value_size;
    
    return 0;
}

size_t int_hash_table_keys(const IntHashTable* htable, int (*keys)[], size_t max_keys) {
    size_t key_count = 0;
    IntKVPair* pair = &htable->buckets.ptr[0];
    IntKVPair* end  = &htable->buckets.ptr[htable->buckets.length];

    for(; pair != end && key_count < max_keys; pair++) {
        if(!pair->key) {
            continue;
        }

        for(IntKVPair *ptr=pair; ptr && key_count < max_keys; ptr = pair->next) {
            (*keys)[key_count++] = ptr->key;
        }
    }

    return key_count;
}

int int_hash_table_lookup(const IntHashTable* htable, int key, void** val, size_t *val_size) {
    size_t *s;
    int retval = int_hash_table_lookup_pair(htable, key, NULL, val, &s);

    if(val_size && s) {
        *val_size = *s;
    }

    return retval;
}

int int_hash_table_lookup_pair(const IntHashTable* htable, int key, int** stored_key, void** val, size_t **val_size) {
    unsigned long hash_index = hash_func(SLICE_FROM_ARRAY(ByteSlice, (unsigned char *)&key, sizeof key));

    for(IntKVPair* pair=&htable->buckets.ptr[hash_index]; pair; pair=pair->next) {
        if(pair->value && key == pair->key) {
            if(stored_key) {
                *stored_key = &pair->key;
            }

            if(val) {
                *val = &pair->value;
            }
            
            if(val_size) {
                *val_size = &pair->value_size;
            }

            return 0;
        }
    }

    return SB_NOT_FOUND;
}

/*
int int_hash_table_merge(IntHashTable* htable1, const IntHashTable* htable2) {

}
*/

int int_hash_table_remove(IntHashTable* htable, int key) {
    return int_hash_table_remove_value(htable, key, NULL, NULL);
}

int int_hash_table_remove_value(IntHashTable* htable, int key, void** value, size_t *val_size) {
    unsigned long hash_index = hash_func(SLICE_FROM_ARRAY(ByteSlice, (unsigned char *)key, sizeof(int)));
    IntKVPair *prev = NULL;

    for(IntKVPair* pair=&htable->buckets.ptr[hash_index]; pair; prev=pair, pair=pair->next) {
        if(pair->value && pair->key == key) {
            if(value) {
                *value = pair->value;
            }
            
            if(val_size) {
                *val_size = pair->value_size;
            }

            pair->value = NULL;

            if(!prev) {
                if(pair->next) {
                    *pair = *pair->next;
                } else {
                    memset(pair, 0, sizeof *pair);
                }
            } else {
                prev->next = pair->next;

                allocator_free(&htable->allocator, pair);
            }

            return 0;
        }
    }

    return SB_NOT_FOUND;
}