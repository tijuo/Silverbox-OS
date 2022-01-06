#ifndef KERNEL_SYNC_H
#define KERNEL_SYNC_H

#include <types.h>
#include <util.h>
#include <stdbool.h>
#include <kernel/apic.h>
#include <x86intrin.h>

/** Obtain exclusive access to a lock. If the lock is unavailable, then
  * spin until access is obtained.
  *
  * @param lock The lock.
  */

NON_NULL_PARAMS void lock_acquire(lock_t *lock);

/** Release control of a lock.
  *
  * @param lock The lock.
  */

NON_NULL_PARAMS static inline void lock_release(lock_t *lock) {
  if(*lock == apic_get_id() + 1)
    *lock = 0;
}

/** Determine whether a lock has been locked.
  *
  * @param lock The lock.
  * @return true, if it's locked. false, if it's unlocked.
  */

NON_NULL_PARAMS PURE static inline bool lock_is_locked(const lock_t *lock) {
  return !!*lock;
}

/** Retrieves the owner of a lock.
  *
  * @param lock The lock.
  * @return The APIC id of the processor that owns the lock. -1, if the lock has no owner.
  */

NON_NULL_PARAMS PURE static inline int lock_get_owner(const lock_t *lock) {
  return *lock - 1;
}

/** Perform a busy loop while some condition is true.
  *
  * @param cond Loops while cond is true. When cond evaluates to false, the loop stops.
  */

#define lock_spin(cond) while(cond) { __pause(); }

#endif /* KERNEL_SYNC_H */
