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
  uint64_t rax;
  uint64_t rbx;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t rbp;
  uint64_t rsp;

  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;

  uint16_t cs;
  uint16_t ds;
  uint16_t es;
  uint16_t fs;
  uint16_t gs;
  uint16_t ss;
  uint8_t fault_num;
  uint8_t processor_id;
  tid_t who;

  uint64_t cr0;
  uint64_t cr2;

  uint64_t cr3;
  uint64_t cr4;

  uint64_t cr8;
  uint64_t rip;

  uint64_t error_code;
  uint64_t rflags;
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
