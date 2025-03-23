#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/list.h>
#include <os/msg/message.h>
#include <os/msg/kernel.h>
#include <os/syscalls.h>

WARN_UNUSED NON_NULL_PARAM(1) int send_message(tcb_t *sender, tid_t recipient_tid, uint32_t subject, uint16_t flags, void *send_buffer, size_t send_buffer_length);
WARN_UNUSED NON_NULL_PARAM(1) int receive_message(tcb_t *recipient, tid_t sender_tid, uint16_t flags, void *receive_buffer, size_t receive_buffer_length);
    
NON_NULL_PARAM(1)
void attach_sender_wait_queue(tcb_t* sender, tcb_t *recipient);

NON_NULL_PARAM(1)
void attach_receiver_wait_queue(tcb_t* receiver, tcb_t *sender);

NON_NULL_PARAMS
void detach_sender_wait_queue(tcb_t* sender);

NON_NULL_PARAMS
void detach_receiver_wait_queue(tcb_t* recipient);

#endif /* MESSAGE */
