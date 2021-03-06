.include "asm/asm.h"

.code32
.text

// Bit 7 of CR4 enables global pages

// void setCR0( dword value )
// void setCR3( dword value )
// void setCR4( dword value )
// void setEflags( dword value )
// dword getCR0()
// dword getCR2()
// dword getCR3()
// dword getCR4()
// dword getEflags()

// bool intIsEnabled()


//  Control register operations

EXPORT setCR0
    mov 4(%esp), %eax
    mov %eax, %cr0
    ret

EXPORT setCR3
    mov 4(%esp), %eax
    mov %eax, %cr3
    ret

EXPORT setCR4
    mov 4(%esp), %eax
    mov %eax, %cr4
    ret

EXPORT getCR0
    mov %cr0, %eax
    ret

EXPORT getCR2
    mov %cr2, %eax
    ret

EXPORT getCR3
    mov %cr3, %eax
    ret

EXPORT getCR4
    mov %cr4, %eax
    ret

EXPORT getEflags
    pushf
    pop %eax
    ret

EXPORT setEflags
    pushl 4(%esp)
    popf
    ret

// Interrupt related operations

EXPORT intIsEnabled
    call getEflags

    shr $8, %eax
    and $1, %eax

    ret
