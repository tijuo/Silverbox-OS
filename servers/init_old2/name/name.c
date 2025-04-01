#include <oslib.h>
#include <os/services.h>
#include <os/driver.h>
#include <string.h>
#include "name.h"
#include <os/vfs.h>
#include <os/msg/init.h>
#include <limits.h>
#include <os/ostypes/string.h>

typedef union {
    struct Device device;
    struct VFS_Filesystem fs;
    tid_t tid;
} NameEntry;

typedef enum { THREAD, DEVICE, FS } NameEntryType;

struct NameRecord {
    NameEntryType name_type;
    NameEntry entry;
    String name;
};

StringHashTable thread_names, device_names, fs_names, device_table, fs_table;

static int name_register_thread(char* name, tid_t tid);
static int name_register_fs(char* name, struct VFS_Filesystem* fs);

static int name_register_major(int major, const struct Device* device);
struct Device* name_unregister_major(int major);
struct Device* name_lookup_major(int major);

static int _registerFs(struct VFS_Filesystem* fs);
static struct VFS_Filesystem* _unregisterFs(char* name);

struct ThreadMapping {
    char name[MAX_NAME_LEN + 1];
    tid_t tid;
};

int name_register_major(int major, const struct Device* device)
{
    if(major < 0 || major > USHRT_MAX) {
        return -1;
    }

    char key[6] = { 0 };

    itoa(major, key, 10);

    return string_hash_table_insert(&device_table, STR(key), device);
}

struct Device* name_unregister_major(int major)
{
    if(major < 0 || major > USHRT_MAX) {
        return -1;
    }

    struct Device* dev;
    char m[6] = { 0 };
    char* storedName;

    itoa(major, m, 10);

    // must remove key

    if(string_hash_table_remove_pair(&device_table, STR(m), &storedName, (void**)&dev) == 0) {
        free(storedName);
        return dev;
    } else
        return NULL;
}

static int _registerFs(struct VFS_Filesystem* fs)
{
    return (!fs || string_hash_table_insert(&fs_table, fs->name, fs) != 0) ? -1 : 0;
}

static struct VFS_Filesystem* _unregisterFs(char* name)
{
    struct VFS_Filesystem* fs;

    if(string_hash_table_lookup(&fs_table, name, (void**)&fs) != 0)
        return NULL;

    return (string_hash_table_remove(&fs_table, name) == 0) ? fs : NULL;
}

static int name_register_fs(char* name, struct VFS_Filesystem* fs)
{
    struct NameRecord* record;

    record = malloc(sizeof * record);

    if(!record)
        return -1;

    record->name[MAX_NAME_LEN] = '\0';
    strncpy(record->name, name, MAX_NAME_LEN);
    record->entry.fs = *fs;
    record->naem_type = FS;

    if(string_hash_table_insert(&fs_names, name, record) != 0) {
        free(record);
        return -1;
    }

    return 0;
}

struct Device* name_lookup_major(int major)
{
    struct Device* dev;
    char m[5];

    itoa(major, m, 10);

    return (string_hash_table_lookup(&device_table, m, (void**)&dev) == 0) ? dev : NULL;
}

void name_register(msg_t* request, msg_t* response)
{
    struct RegisterNameRequest* nameRequest = (struct RegisterNameRequest*)&request->data;
    struct ThreadMapping* mapping = malloc(sizeof * mapping);

    mapping->name[MAX_NAME_LEN] = '\0';
    strncpy(mapping->name, nameRequest->name, MAX_NAME_LEN);
    mapping->tid = request->sender;

    int result = string_hash_table_insert(&thread_names, mapping->name, mapping);

    response->subject = (result == 0 ? RESPONSE_OK : RESPONSE_FAIL);
}

void name_lookup(msg_t* request, msg_t* response)
{
    struct LookupNameRequest* nameRequest = (struct LookupNameRequest*)&request->data;
    struct ThreadMapping* mapping;
    char name[MAX_NAME_LEN + 1];

    name[MAX_NAME_LEN] = '\0';
    strncpy(name, nameRequest->name, MAX_NAME_LEN);

    if(string_hash_table_lookup(&thread_names, name, (void**)&mapping) == 0) {
        struct LookupNameResponse* nameResponse = (struct LookupNameResponse*)&response->data;

        nameResponse->tid = mapping->tid;
        response->subject = RESPONSE_OK;
    } else
        response->subject = RESPONSE_FAIL;
}

void name_unregister(msg_t* request, msg_t* response)
{
    struct UnregisterNameRequest* nameRequest = (struct UnregisterNameRequest*)&request->data;
    struct ThreadMapping* mapping;
    char name[MAX_NAME_LEN + 1];

    name[MAX_NAME_LEN] = '\0';
    strncpy(name, nameRequest->name, MAX_NAME_LEN);

    if(string_hash_table_remove_pair(&thread_names, name, NULL, (void**)&mapping) != 0) {
        response->subject = RESPONSE_FAIL;
    } else {
        free(mapping);
        response->subject = RESPONSE_OK;
    }
}

