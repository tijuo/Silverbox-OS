.code32
.section .stacks, "aw", @nobits

// These are the extra kernel stacks to be used when a thread blocks while in kernel mode

.rept 1048576
.byte 0
.endr
