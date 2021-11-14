#include <oslib.h>
#include <os/services.h>
#include <os/message.h>
#include <stdlib.h>
#include <os/syscalls.h>
#include <os/msg/message.h>
#include <os/msg/kernel.h>

#define EMPTY_REQUEST_MSG(subj, recv) { .subject = subj, .sender = NULL_TID, .recipient = recv, .buffer = NULL, .bufferLen = 0, .flags = 0, .bytesTransferred = 0 }
#define REQUEST_MSG(subj, recv, request) { .subject = subj, .sender = NULL_TID, .recipient = recv, .buffer = &request, .bufferLen = sizeof request, .flags = 0, .bytesTransferred = 0 }
#define RESPONSE_MSG(response)  { .subject = 0, .sender = NULL_TID, .recipient = NULL_TID, .buffer = &response, .bufferLen = sizeof response, .flags = 0, .bytesTransferred = 0 }

int irqBind(unsigned int irq) {
  msg_t requestMsg = EMPTY_REQUEST_MSG(BIND_IRQ_MSG, IRQ0_TID + irq)
  ;
  requestMsg.flags |= MSG_SYSTEM;

  return sys_send(&requestMsg) == ESYS_OK ? 0 : -1;
}

int irqUnbind(unsigned int irq) {
  msg_t requestMsg = EMPTY_REQUEST_MSG(UNBIND_IRQ_MSG, IRQ0_TID + irq)
  ;
  requestMsg.flags |= MSG_SYSTEM;

  return sys_send(&requestMsg) == ESYS_OK ? 0 : -1;
}

int irqEoi(unsigned int irq) {
  msg_t requestMsg = EMPTY_REQUEST_MSG(EOI_MSG, IRQ0_TID + irq)
  ;
  requestMsg.flags |= MSG_SYSTEM;

  return sys_send(&requestMsg) == ESYS_OK ? 0 : -1;
}
