#ifndef OS_MSG_KERNEL_H
#define OS_MSG_KERNEL_H

#include <types.h>
#include <os/msg/message.h>

#define IRQ0_TID            1

#define EXCEPTION_MSG           0xF0
#define IRQ_MSG                 0xF1
#define EXIT_MSG                0xF2
#define EOI_MSG                 0xF3
#define MEMORY_MSG              0xF4
#define BIND_IRQ_MSG            0xF5
#define UNBIND_IRQ_MSG          0xF6

struct ExceptionMessage
{
  tid_t who;
  int errorCode;
  int faultAddress;
  int intNum;
};

struct ExitMessage
{
  tid_t who;
  int  statusCode;
};

struct IrqMessage
{
  int irq;
};

#endif /* OS_MSG_KERNEL_H */
