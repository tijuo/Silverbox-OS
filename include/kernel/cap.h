#ifndef KERNEL_CAP_H
#define KERNEL_CAP_H

#include <stdint.h>
#include <kernel/thread.h>
#include <os/cap.h>

/*
/// Indicates that a capability table entry is valid and currently in use.
#define CAP_FLAG_VALID      0x80
*/

/** Retrieves the capability entry that corresponds to a capability token (assumes that the token is valid).

    @param tcb_ptr The TCB of the thread (must be a pointer).
    @param index The index into the capability table (indicies start at 1).
    @return A pointer to the capability entry as a cap_entry_t.
*/

#define CAP_ENTRY(tcb_ptr, index)   (tcb_ptr)->cap_table[(size_t)((index)-1)]

#define CAP_TYPE_ROOT       0x80
#define CAP_TYPE_GRANT      0x40
#define CAP_TYPE_FREE       0x00
#define CAP_TYPE_MASK       0xC0

/// The maximum number of entries allowed in a capability table.

#define CAP_TABLE_MAXLEN    ((size_t)UINT16_MAX)

typedef uint8_t cap_restype_t;
typedef uint8_t cap_rights_t;

typedef union cap_resource {
    tid_t tid;
    uint8_t irq;
    uint8_t except_num;
} cap_res_t;

typedef struct cap_token {
    tid_t tid;
    cap_index_t table_index;
} cap_token_t;

struct cap_grant_entry {
    tid_t grantee;
    cap_token_t grantor_token;
};

struct cap_root_entry {
    cap_res_t resource;
    tid_t owner;
};

typedef struct cap_entry {
    union {
        struct cap_grant_entry grant;
        struct cap_root_entry root;
    };

    cap_index_t next_index;
    cap_index_t prev_index;
    cap_rights_t rights;    // upper two bits are for capability type
    cap_restype_t res_type;
    uint8_t _padding[4];
} cap_entry_t;


static_assert(IS_POWER_OF_TWO(sizeof(cap_entry_t)), "Capability entry size must be a power of 2");

NON_NULL_PARAM(1) void cap_table_init(tcb_t *tcb, void *addr, size_t capacity);
NON_NULL_PARAMS int cap_revoke(tcb_t *tcb, cap_token_t token);
NON_NULL_PARAMS int cap_grant(tcb_t *restrict grantor,
        tcb_t *restrict grantee, cap_token_t grantor_cap, cap_rights_t rights_mask, cap_token_t *grantee_token);
NON_NULL_PARAMS int cap_create_right(tcb_t *tcb, cap_restype_t res_type,
                                     cap_res_t resource, cap_rights_t rights, cap_token_t *token);
NON_NULL_PARAM(1) bool cap_has_right(tcb_t *tcb, cap_token_t token, cap_restype_t res_type, cap_res_t, cap_rights_t rights);

#endif /* KERNEL_CAP_H */
