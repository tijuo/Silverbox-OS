#ifndef OS_MSG_KERNEL_H
#define OS_MSG_KERNEL_H

#include <types.h>

#define EXCEPTION_MSG           0xF0
#define IRQ_MSG                 0xF1
#define EXIT_MSG                0xF2

#define MSG_KERNEL  0x80000000

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

#endif /* OS_MSG_KERNEL_H */
