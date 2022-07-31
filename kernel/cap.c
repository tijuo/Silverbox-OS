#include <kernel/cap.h>
#include <kernel/bits.h>
#include <kernel/error.h>
#include <kernel/debug.h>

_Static_assert(IS_POWER_OF_TWO(sizeof(cap_entry_t)), "Capability entry size must be a power of 2.");
_Static_assert(sizeof(struct cap_grant_entry) == sizeof(struct cap_free_entry), "Grant entries must be the same size as free entries.");

/**
  Initializes a thread's capability table.

  @param tcb The thread for which the table will be initialized.
  @param addr The address of the capability table. If NULL, then
  the table will be marked as empty.
  @param capacity The maximum number of entries for the capability
  table. If addr is non-NULL, then addr must point to a buffer that
  can store at least capacity cap_entry_t entries.
*/

void cap_table_init(tcb_t *tcb, void *addr, size_t capacity) {
    tcb->cap_table = (cap_entry_t *)addr;

    if(addr == NULL)
        tcb->cap_table_capacity = 0;
    else {
        tcb->cap_table = addr;
        tcb->cap_table_capacity = capacity > UINT16_MAX ? UINT16_MAX : (uint16_t)capacity;
        cap_entry_t *entry = tcb->cap_table;

        entry->permissions = 0;
        entry->free.next = NULL_CAP;
        entry++;

        for(cap_t i=1; i < tcb->cap_table_capacity; i++, entry++) {
            entry->permissions = 0;
            entry->free.next = (i+1);
        }

        if(tcb->cap_table_capacity == UINT16_MAX)
            tcb->cap_table[UINT16_MAX].free.next = NULL_CAP;
    }
}

/**
  Verifies that a thread has a set of permissions according to its capability.

  @param tcb The thread.
  @param c The capability.
  @param permissions The set of permissions represented at bit flags.
  @return true, if all permissions are possessed. false, otherwise.
*/

bool cap_has_permissions(tcb_t *tcb, cap_t c, uint32_t permissions) {
    return (c != NULL_CAP && c < tcb->cap_table_capacity
        && get_tid(tcb) == tcb->cap_table[c].grant.grantee
        && (permissions & CAP_PERM_MASK)) ? true : false;
}

/**
  Invalidates all grants that were issued using this capability, including the grants of those grants, and so on.
  The original grantor still posseses its capability.

  @param tcb The thread's grantor.
  @param c The grantor's capability that will be revoked from all grantees.
  @return E_OK, on success. E_FAIL, if a capability wasn't able to be revoked from a grantee,
  E_PERM, if the grantor does not actually possess the capability.
*/

int cap_revoke(tcb_t *tcb, cap_t c) {
    if(cap_is_valid(tcb, c)) {
        tid_t grantor_tid = get_tid(tcb);
        cap_entry_t *cap_entry = tcb->cap_table;

        for(size_t i=0; i < (size_t)tcb->cap_table_capacity; i++, cap_entry++) {
            if(IS_FLAG_SET(cap_entry->permissions, CAP_FLAG_VALID)
                    && cap_entry->grant.grantor == grantor_tid
                    && cap_entry->grant.grantor_cap == c) {
                if(IS_ERROR(cap_revoke(get_tcb(cap_entry->grant.grantee), cap_entry->grant.grantee_cap)))
                    RET_MSG(E_FAIL, "Failed to recursively revoke capabilities.");
                else {
                    tcb_t *grantee_tcb = get_tcb(cap_entry->grant.grantee);
                    grantee_tcb->cap_table[(size_t)cap_entry->grant.grantee_cap].permissions = 0;
                }
            }
        }

        return E_OK;
    }
    else
      RET_MSG(E_PERM, "Unable to revoke capability.");
}

/**
  Grant a set of permissions to another thread.

  @param grantor The thread granting the permission(s). Must not be NULL nor
  identical to grantee.
  @param grantee The thread that will gain the permission(s). Must neither be NULL
  nor identical to grantor.
  @param grantor_cap The grantor's capability that will be granted.
  @param permissions The set of permissions to be granted. Each permission is a one-bit
  flag.
  @param grantee_cap A pointer to the grantee's capability. Must not be NULL.
  @return E_OK, on success with grantee_cap pointing to the granted capability to be used
  by the grantee only. E_PERM, if the grantor does not have permission to perform the grant.
  E_FAIL, if the capability table for the grantee is full.
*/

int cap_grant(tcb_t *restrict grantor, tcb_t *restrict grantee, cap_t grantor_cap, uint32_t permissions,
              cap_t *grantee_cap) {
    if(cap_has_permissions(grantor, grantor_cap, permissions | CAP_FLAG_GRANT | CAP_FLAG_VALID)) {
        cap_entry_t *grantor_entry = &grantor->cap_table[(size_t)grantor_cap];
        cap_entry_t *grantee_entry = grantee->cap_table;

        for(cap_t i=0; i < grantee->cap_table_capacity; i++, grantee_entry++) {
            if(!IS_FLAG_SET(grantee_entry->permissions, CAP_FLAG_VALID)) {
                grantee_entry->permissions = permissions & grantor_entry->permissions;
                grantee_entry->grant.grantee = get_tid(grantee);
                grantee_entry->grant.grantor = get_tid(grantor);
                grantee_entry->grant.grantor_cap = grantor_cap;
                *grantee_cap = grantee_entry->grant.grantee_cap = i;

                return E_OK;
            }
        }

        RET_MSG(E_FAIL, "Unable to grant permission(s). Capability table is full.");
    }
    else {
        RET_MSG(E_PERM, "Unable to grant permission(s).");
    }
}
