.code32

.include "asm/context.h"

.text

IMPORT handleCPUException
IMPORT timerInt
IMPORT _syscall
IMPORT handleIRQ

.macro ECODE_CPU_EX_HANDLER err_code
  push %ecx
  mov 4(%esp), %ecx
  mov %eax, 4(%esp)
  mov %ecx, -28(%esp)  // Move error code out of the way (to be used in a future interrupt handler)
  ECODE_SAVE_STATE
  sub  $4, %esp
  push $\err_code

  call handleCPUException

  add $8, %esp
  RESTORE_STATE
  iret
.endm

.macro CPU_EX_HANDLER int_num
  SAVE_STATE
  push $0
  push $\int_num

  call handleCPUException

  add $8, %esp
  RESTORE_STATE
  iret
.endm

.macro INVALID_EX_HANDLER int_num
  SAVE_STATE
  push $0xBAD1
  push $\int_num

  call handleCPUException

  add $8, %esp
  RESTORE_STATE
  iret
.endm

.macro IRQ_HANDLER irq_num
  SAVE_STATE
  push $\irq_num

  call handleIRQ

  add $4, %esp
  RESTORE_STATE
  iret
.endm


EXPORT intHandler0
  CPU_EX_HANDLER 0

EXPORT intHandler1
  CPU_EX_HANDLER 1

EXPORT intHandler2
  CPU_EX_HANDLER 2

EXPORT intHandler3
  CPU_EX_HANDLER 3

EXPORT intHandler4
  CPU_EX_HANDLER 4

EXPORT intHandler5
  CPU_EX_HANDLER 5

EXPORT intHandler6
  CPU_EX_HANDLER 6

EXPORT intHandler7
  CPU_EX_HANDLER 7

EXPORT intHandler8
  ECODE_CPU_EX_HANDLER 8

EXPORT intHandler9
  CPU_EX_HANDLER 9

EXPORT intHandler10
  ECODE_CPU_EX_HANDLER 10

EXPORT intHandler11
  ECODE_CPU_EX_HANDLER 11

EXPORT intHandler12
  ECODE_CPU_EX_HANDLER 12

EXPORT intHandler13
  ECODE_CPU_EX_HANDLER 13

EXPORT intHandler14
  ECODE_CPU_EX_HANDLER 14

EXPORT intHandler16
  CPU_EX_HANDLER 16

EXPORT intHandler17
  ECODE_CPU_EX_HANDLER 17

EXPORT intHandler18
  CPU_EX_HANDLER 18

EXPORT intHandler19
  ECODE_CPU_EX_HANDLER 19

EXPORT invalidIntHandler
  INVALID_EX_HANDLER 0

EXPORT irq1Handler
    IRQ_HANDLER 1

EXPORT irq2Handler
    IRQ_HANDLER 2

EXPORT irq3Handler
    IRQ_HANDLER 3

EXPORT irq4Handler
    IRQ_HANDLER 4

EXPORT irq5Handler
    IRQ_HANDLER 5

EXPORT irq6Handler
    IRQ_HANDLER 6

EXPORT irq7Handler
    IRQ_HANDLER 7

EXPORT irq8Handler
    IRQ_HANDLER 8

EXPORT irq9Handler
    IRQ_HANDLER 9

EXPORT irq10Handler
    IRQ_HANDLER 10

EXPORT irq11Handler
    IRQ_HANDLER 11

EXPORT irq12Handler
    IRQ_HANDLER 12

EXPORT irq13Handler
    IRQ_HANDLER 13

EXPORT irq14Handler
    IRQ_HANDLER 14

EXPORT irq15Handler
    IRQ_HANDLER 15

EXPORT timerHandler
    IRQ_HANDLER 0

EXPORT syscallHandler
    SAVE_STATE
    call _syscall
    RESTORE_STATE
    iret
