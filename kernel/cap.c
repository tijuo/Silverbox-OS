#include <assert.h>
#include <kernel/cap.h>
#include <kernel/bits.h>
#include <kernel/error.h>
#include <kernel/debug.h>

#define CAP_TYPE(entry)  ((entry)->rights & CAP_TYPE_MASK)

static inline cap_entry_t *get_token_entry(cap_token_t token) {
    return &CAP_ENTRY(get_tcb(token.tid), token.table_index);
}

static NON_NULL_PARAMS bool cap_is_valid(tcb_t *tcb, cap_token_t token);
static NON_NULL_PARAMS void cap_insert_entry(tcb_t *tcb, cap_index_t index, bool use_free_list);
static NON_NULL_PARAMS void cap_remove_entry(tcb_t *tcb, cap_index_t index, bool use_free_list);

/* Capability token values range from 1 to CAP_TABLE_MAXLEN, inclusive. Token value 1
 * corresponds to entry 0 in a thread's capability table. Value 2 corresponds to entry
 * 1, and so on.
 */

static NON_NULL_PARAMS void cap_insert_entry(tcb_t *tcb, cap_index_t index, bool use_free_list) {
    cap_entry_t *entry = &CAP_ENTRY(tcb, index);
    cap_index_t head_index = use_free_list ? tcb->cap_free_head : tcb->cap_entry_head;

    entry->next_index = head_index;
    entry->prev_index = CAP_NULL_INDEX;

    if(head_index != CAP_NULL_INDEX)
        CAP_ENTRY(tcb, head_index).prev_index = index;

    if(use_free_list)
        tcb->cap_free_head = index;
    else
        tcb->cap_entry_head = index;
}

static NON_NULL_PARAMS void cap_remove_entry(tcb_t *tcb, cap_index_t index, bool use_free_list) {
    cap_entry_t *entry = &CAP_ENTRY(tcb, index);

    if(entry->prev_index != CAP_NULL_INDEX)
        CAP_ENTRY(tcb, entry->prev_index).next_index = entry->next_index;
    else if(use_free_list)
        tcb->cap_free_head = entry->next_index;
    else
        tcb->cap_entry_head = index;

    if(entry->next_index != CAP_NULL_INDEX)
        CAP_ENTRY(tcb, entry->next_index).prev_index = entry->prev_index;
}

/**
    Initializes a thread's capability table.

    @param tcb The TCB of the thread for which the table will be initialized.
    @param addr The address of the capability table. If NULL, then
    the table will be marked as empty.
    @param capacity The maximum number of entries for the capability
    table. If addr is non-NULL, then addr must point to a buffer that
    can store at least capacity cap_entry_t entries.
*/

void cap_table_init(tcb_t *tcb, void *addr, size_t capacity) {
    if(addr == NULL || capacity == 0) {
        tcb->cap_table = NULL;
        tcb->cap_table_capacity = 0;
    } else {
        tcb->cap_table = addr;
        tcb->cap_table_capacity = (uint16_t)(capacity > CAP_TABLE_MAXLEN ? CAP_TABLE_MAXLEN : capacity);
        cap_entry_t *entry = &CAP_ENTRY(tcb, 1);

        entry->prev_index = CAP_NULL_INDEX;

        for(cap_index_t i=0; (size_t)i < (size_t)tcb->cap_table_capacity; i++, entry++) {
            entry->rights = CAP_TYPE_FREE;
            entry->next_index = (i+2);
            entry->prev_index = i;
        }

        if(tcb->cap_table_capacity == CAP_TABLE_MAXLEN)
            CAP_ENTRY(tcb, CAP_TABLE_MAXLEN).next_index = CAP_NULL_INDEX;
    }
}

/**
    Give a right to a resource to a thread and create a new capability token.

    @param tcb The TCB of the thread for which the right is to be given.
    @param right The right.
    @param resource The resource for which the right is to be given.
    @param rights A set of right flags pertaining to the capability.
    @param token A pointer to which the new capability token will be saved.

    @return E_OK, on success. E_FAIL, on failure.
*/

int cap_create_right(tcb_t *tcb, cap_restype_t res_type, cap_res_t resource,
                     cap_rights_t rights, cap_token_t *token) {
    if(tcb->cap_free_head == CAP_NULL_INDEX)
        RET_MSG(E_FAIL, "Unable to create capability token. No more free capability entries are available.");
    else {
        // Grab the free entry from the head of the free list

        token->table_index = tcb->cap_free_head;
        token->tid = get_tid(tcb);

        cap_remove_entry(tcb, token->table_index, true);

        cap_entry_t *new_entry = get_token_entry(*token);

        new_entry->rights = CAP_TYPE_ROOT | (rights & ~CAP_TYPE_MASK);
        new_entry->res_type = res_type;
        new_entry->root.resource = resource;

        // ...then add the entry to the used list

        cap_insert_entry(tcb, token->table_index, false);

        return E_OK;
    }
}

/**
    Determines whether a capability token is valid and actually owned by a thread.

    @param tcb The TCB of the thread.
    @param token The capability token.
    @return true, if the given capability token is valid for a thread. false, otherwise.
*/

bool cap_is_valid(tcb_t *tcb, cap_token_t token) {
    tcb_t *grantor = get_tcb(token.tid);

    if(token.table_index != CAP_NULL_INDEX && (size_t)token.table_index <= (size_t)grantor->cap_table_capacity) {
        cap_entry_t *entry = get_token_entry(token);
        tid_t tid = get_tid(tcb);

        switch(CAP_TYPE(entry)) {
            case CAP_TYPE_ROOT:
                return entry->root.owner == tid;
            case CAP_TYPE_GRANT:
                return entry->grant.grantee == tid;
            case CAP_TYPE_FREE:
            default:
                return false;
        }
    }
    else
        return false;
}

/**
    Verifies that a thread has a right to a resource according to its capability token.

    @param tcb The TCB of the thread.
    @param token The capability token.
    @param right The right.
    @param resource The resource to which the thread has the right.
    @return true, if the thread possesses the right. false, otherwise.
*/

bool cap_has_right(tcb_t *tcb, cap_token_t token, cap_restype_t res_type, cap_res_t resource, cap_rights_t rights) {
    if(cap_is_valid(tcb, token)) {
        cap_entry_t *entry = get_token_entry(token);

        if((entry->rights & rights) != rights)
            return false;

        while(1) {
            switch(CAP_TYPE(entry)) {
                case CAP_TYPE_GRANT:
                        entry = &CAP_ENTRY(get_tcb(entry->grant.grantor_token.tid), entry->grant.grantor_token.table_index);
                        break;
                case CAP_TYPE_ROOT:
                    if(res_type != entry->res_type)
                        return false;
                    else if(IS_FLAG_SET(entry->rights, CAP_RIGHT_UNIVERSAL))
                        return true;
                    else {
                        switch(entry->res_type) {
                            case CAP_RES_THREAD:
                                return resource.tid == entry->root.resource.tid;
                            case CAP_RES_IRQ:
                                return resource.irq == entry->root.resource.irq;
                            case CAP_RES_EXCEPT:
                                return resource.except_num == entry->root.resource.except_num;
                            default:
                                return true;
                        }
                    }
                default:
                    return false;
            }
        }
    } else
        return false;
}

/**
    Invalidates all grants that were issued using this capability token,
    including the grants of those grants, and so on.
    The original grantor still posseses the token with its right.

    @param tcb The TCB of the original grantor.
    @param token The grantor's capability token for which rights will be revoked from all grantees.
    @return E_OK, on success. E_FAIL, if a capability wasn't able to be revoked from a grantee,
    E_PERM, if the grantor does not actually possess the capability token.
*/

int cap_revoke(tcb_t *tcb, cap_token_t token) {
    if(cap_is_valid(tcb, token)) {
        for(cap_index_t t=tcb->cap_entry_head; t != CAP_NULL_INDEX; t=CAP_ENTRY(tcb, t).next_index) {
            cap_entry_t *cap_entry = &CAP_ENTRY(tcb, t);

            if(CAP_TYPE(cap_entry) == CAP_TYPE_GRANT && cap_entry->grant.grantor_token.tid == token.tid
                && cap_entry->grant.grantor_token.table_index == token.table_index) {
                tcb_t *grantee_tcb = get_tcb(cap_entry->grant.grantee);
                cap_token_t grantee_token = {
                    .tid = get_tid(tcb),
                    .table_index = t
                };

                if(IS_ERROR(cap_revoke(grantee_tcb, grantee_token)))
                    RET_MSG(E_FAIL, "Failed to recursively revoke capabilities.");
                else {
                    cap_remove_entry(grantee_tcb, t, false);
                    cap_entry->rights = 0;
                    cap_insert_entry(grantee_tcb, t, true);
                }
            }
        }

        return E_OK;
    } else
        RET_MSG(E_PERM, "Unable to revoke capability.");
}

/**
    Grant a right to a resource to another thread.

    @param grantor The thread granting the right. Must neither be NULL nor
    identical to grantee.
    @param grantee The thread that will acquire the right. Must neither be NULL
    nor identical to grantor.
    @param grantor_token The grantor's capability token for which the right will be granted.
    @param rights_mask A bit mask used to limit which right flags are transferred to the new capability token.
    @param grantee_token A pointer to the grantee's capability token. Must not be NULL.
    @return E_OK, on success with grantee_token updated to indicate the grantee's capability token
    (it can only be used by the grantee).
    E_PERM, if the grantor does not have permission to perform the grant.
    E_FAIL, if the capability table for the grantee is full.
*/

int cap_grant(tcb_t *restrict grantor, tcb_t *restrict grantee, cap_token_t grantor_token,
              cap_rights_t rights_mask, cap_token_t *grantee_token) {
    if(cap_is_valid(grantor, grantor_token)) {
        cap_entry_t *grantor_entry = &CAP_ENTRY(get_tcb(grantor_token.tid), grantor_token.table_index);

        if(!IS_FLAG_SET(grantor_entry->rights, CAP_TYPE_GRANT))
            RET_MSG(E_PERM, "Thread does not have permission to grant right.");
        else if(grantor->cap_free_head == CAP_NULL_INDEX)
            RET_MSG(E_FAIL, "Unable to grant right. Capability table is full.");

        grantee_token->table_index = grantor->cap_free_head;
        grantee_token->tid = get_tid(grantor);

        cap_remove_entry(grantor, grantor->cap_free_head, true);

        cap_entry_t *grantee_entry = &CAP_ENTRY(grantor, grantee_token->table_index);

        assert(CAP_TYPE(grantee_entry) != CAP_TYPE_FREE);

        grantee_entry->res_type = grantor_entry->res_type;
        grantee_entry->rights = CAP_TYPE_GRANT | (grantor_entry->rights & ~CAP_TYPE_MASK & rights_mask);
        grantee_entry->grant.grantee = get_tid(grantee);
        grantee_entry->grant.grantor_token = grantor_token;

        return E_OK;
    } else
        RET_MSG(E_PERM, "Unable to grant right.");
}
