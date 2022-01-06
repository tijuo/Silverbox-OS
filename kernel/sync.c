#include <kernel/sync.h>

void lock_acquire(lock_t *lock) {
  int my_id = apic_get_id() + 1;
  int lock_owner;

  while(*lock != my_id) {
    lock_spin(*lock);

    asm("lock cmpxchgl %1, %2"
      : "=a" (lock_owner)
      : "r"(my_id), "m"(*lock), "a"(0)
      : "cc", "memory");
  }
}
