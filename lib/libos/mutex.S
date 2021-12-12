.file "mutex.s"
.text

.global mutex_lock
.type	mutex_lock, @function

.global mutex_is_locked
.type	mutex_is_locked, @function

.global mutex_unlock
.type	mutex_unlock, @function

mutex_lock:
	mov $1, %rax
	jmp _mutex_op

mutex_unlock:
	xor %eax, %eax
	jmp _mutex_op

_mutex_op:
	lock xchg %rax, (%rdi)
	ret

mutex_is_locked:
	mov (%rdi), %rax
	ret
