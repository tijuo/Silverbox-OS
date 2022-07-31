#ifndef KERNEL_CAP_H
#define KERNEL_CAP_H

#include <stdint.h>
#include <kernel/thread.h>

#define CAP_PERM_MASK       0x3FFFFFFFu
#define CAP_PERM_SPAWN      (1u << 0) // Spawn a new thread in same address space
#define CAP_PERM_SPAWN_ISO  (1u << 1) // Spawn an new thread in a new address space
#define CAP_PERM_TCB_READ   (1u << 2) // Read TCBs
#define CAP_PERM_TCB_WRITE  (1u << 3) // Write TCBs
#define CAP_PERM_MEM        (1u << 4) // Perform map, grant, flush operations
#define CAP_PERM_REG_IRQ    (1u << 5) // IRQ hanldler registration
#define CAP_PERM_REG_EH     (1u << 6) // Exception handler registration
#define CAP_FLAG_VALID      (1u << 31)
#define CAP_FLAG_GRANT      (1u << 30)

#define cap_is_valid(tcb, c) cap_has_permissions(tcb, c, CAP_FLAG_VALID)

#define NULL_CAP      0

struct cap_grant_entry {
  tid_t grantor;
  tid_t grantee;
  cap_t grantor_cap;
  cap_t grantee_cap;
};

struct cap_free_entry {
  tid_t next;
  uint16_t resd[3];
};

typedef struct cap_entry {
  uint32_t permissions;
  union {
    struct cap_grant_entry grant;
    struct cap_free_entry free;
  };
  uint32_t resd;
} cap_entry_t;

NON_NULL_PARAMS bool cap_has_permissions(tcb_t *tcb, cap_t c, uint32_t permissions);
NON_NULL_PARAMS int cap_revoke(tcb_t *tcb, cap_t c);
NON_NULL_PARAM(2) NON_NULL_PARAM(5) int cap_grant(tcb_t * restrict grantor, tcb_t * restrict grantee, cap_t grantor_cap,
  uint32_t permissions, cap_t *grantee_cap);
NON_NULL_PARAM(1) void cap_table_init(tcb_t *tcb, void *addr, size_t capacity);

#endif /* KERNEL_CAP_H */
