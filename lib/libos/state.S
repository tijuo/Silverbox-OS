.file "state.s"
.text
.globl _restore_context
.type	_restore_context, @function
.globl _switch_context
.type _switch_context, @function

# _switch_context and _restore_context can switch can switch between
# stacks and instruction pointers, but the correct stack address
# is yet to be determined.

# _switch_context( Registers *this, Registers *other )

_switch_context:
	push	%ebp
	movl	%esp, %ebp

	cmpl	$0, 8(%ebp)
	je	.ret1
	cmpl	$0, 12(%ebp)
	je	.ret1

# Save the current registers

	movl	8(%ebp), %esp
	add	$32, %esp
	pusha
	movl	8(%ebp), %eax
	movl	%ebp, 12(%eax)	# Saves esp?

# Restores the other context

	movl	12(%ebp), %esp
	popa
	sub	$20, %esp
	movl	(%esp), %esp	# Restores esp?

.ret1:
	leave
	ret

_restore_context:
	push	%ebp
	movl	%esp, %ebp

	cmpl	$0, 8(%ebp)
	je	.ret2

	movl	8(%ebp), %esp
	popa

	sub	$20, %esp
	movl	(%esp), %esp

.ret2:
	leave
	ret
