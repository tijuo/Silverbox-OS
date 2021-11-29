#ifndef OS_MSG_KERNEL_H
#define OS_MSG_KERNEL_H

#include <types.h>
#include <os/msg/message.h>

#define IRQ0_TID            1

#define EXCEPTION_MSG           0xFFFFFFFFu
#define EXIT_MSG		            0xFFFFFFFEu
#define MEMORY_MSG              0xFFFFFFFDu

struct ExceptionMessage
{
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;

  uint32_t esi;
  uint32_t edi;
  uint32_t ebp;
  uint32_t esp;

  uint16_t cs;
  uint16_t ds;
  uint16_t es;
  uint16_t fs;
  uint16_t gs;
  uint16_t ss;
  uint32_t eflags;

  uint32_t cr0;
  uint32_t cr2;
  uint32_t cr3;
  uint32_t cr4;

  uint32_t eip;
  uint32_t error_code;
  uint8_t fault_num;
  uint8_t processor_id;
  tid_t who;
};

struct ExitMessage
{
  tid_t who;
  int  status_code;
};

struct IrqMessage
{
  int irq;
};

#endif /* OS_MSG_KERNEL_H */
