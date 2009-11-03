  .file "mutex.s"
  .text

.global mutex_lock
.type	mutex_lock, @function

.global mutex_unlock
.type	mutex_unlock, @function

mutex_lock:
  push %ebp
  mov %esp, %ebp
  mov $1, %eax
  jmp _mutex_op

mutex_unlock:
  push %ebp
  mov %esp, %ebp
  mov $0, %eax
  jmp _mutex_op

_mutex_op:
  mov 8(%ebp), %ecx
  lock xchg %eax, (%ecx)
  leave
  ret
