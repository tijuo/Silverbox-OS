#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/list.h>
#include <os/msg/kernel.h>
#include <os/syscalls.h>

NON_NULL_PARAMS
int send_and_receive_message(tcb_t* sender, tid_t recipient_tid,
    uint32_t subject, uint16_t flags,
    SysMessageArgs *args);
    
NON_NULL_PARAMS
int attach_send_wait_queue(tcb_t* sender, tid_t recipient);

NON_NULL_PARAMS
int attach_receive_wait_queue(tcb_t* receiver, tid_t sender);

NON_NULL_PARAMS
int detach_send_wait_queue(tcb_t* sender);

NON_NULL_PARAMS
int detach_receive_wait_queue(tcb_t* receiver);

#endif /* MESSAGE */
