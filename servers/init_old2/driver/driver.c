#include "driver.h"
#include <os/driver.h>

IntHashTable drivers;
StringHashTable driver_majors;

// XXX: This isn't thread-safe
Allocator *driver_allocator = &GLOBAL_ALLOCATOR;

int driver_init(void) {
    if(string_hash_table_init(&driver_majors, 31, &GLOBAL_ALLOCATOR) != 0) {
        return -1;
    }

    if(int_hash_table_init(&drivers, 31, &GLOBAL_ALLOCATOR) != 0) {
        string_hash_table_destroy(&driver_majors);
        return -1;
    }

    return 0;
}

void driver_cleanup(void) {
    string_hash_table_destroy(&driver_majors);
    int_hash_table_destroy(&drivers);
}

int driver_register(major_t major, StrSlice name, const DriverOps *ops) {
    if(!ops || major < 0) {
        return SB_FAIL;
    }

    String new_name;
    Driver *driver = NULL;
    major_t *existing_major = NULL;

    if(driver_lookup_major(major) != NULL || driver_lookup_name(name) != INVALID_MAJOR) {
        return SB_CONFLICT;
    }

    if(!string_from_str(&new_name, name, NULL)) {
        return SB_FAIL;
    }

    ByteSlice b = allocator_alloc(driver_allocator, sizeof(Driver), sizeof(int));

    if(!b.ptr) {
        goto destroy_name;
    }

    driver = (Driver *)b.ptr;

    driver->major = major;
    driver->name = new_name;
    driver->ops = *ops;

    if(int_hash_table_insert(&drivers, (int)major, driver, sizeof *driver) != 0) {
        goto destroy_driver;
    }

    if(string_hash_table_insert(&driver_majors, STRING_AS_STR(name), &driver->major, sizeof driver->major) != 0) {
        int_hash_table_remove(&drivers, (int)major);
        goto destroy_driver;
    }

    return 0;

destroy_driver:
    allocator_free(driver_allocator, driver);
destroy_name:
    string_destroy(&new_name);
    return SB_FAIL;
}

int driver_unregister(major_t major) {
    Driver *driver;

    if(major < 0) {
        return SB_FAIL;
    }

    if(int_hash_table_remove_value(&drivers, (int)major, &driver, NULL) == 0) {
        return SB_CONFLICT;
    }

    string_hash_table_remove(&driver_majors, STRING_AS_STR(driver->name));

    string_destroy(&driver->name);
    allocator_free(driver_allocator, driver);

    return 0;
}

Driver *driver_lookup_major(major_t major) {
    Driver *driver;

    if(major >= 0 && int_hash_table_lookup(&drivers, (int)major, (void **)&driver, NULL) == 0) {
        return driver;
    } else {
        return NULL;
    }
}

major_t driver_lookup_name(StrSlice name) {
    major_t *major;

    if(string_hash_table_lookup(&driver_majors, name, (void **)&major, NULL) == 0) {
        return *major;
    } else {
        return INVALID_MAJOR;
    }
}