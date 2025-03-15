#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/list.h>
#include <os/msg/kernel.h>

NON_NULL_PARAMS
int receive_message(tcb_t* recipient, tid_t sender_tid, uint32_t flags);

NON_NULL_PARAMS
int _send_and_receive_message(tcb_t* sender, tid_t recipient_tid, tid_t replier_tid,
    uint32_t subject, uint32_t send_flags,
    uint32_t recv_flags, bool send_only);

NON_NULL_PARAMS
static inline int send_message(tcb_t* sender, tid_t recipient_tid,
    uint32_t subject, uint32_t flags)
{
    return _send_and_receive_message(sender, recipient_tid, NULL_TID, subject, flags,
        0, true);
}

NON_NULL_PARAMS
static inline int send_and_receive_message(tcb_t* sender, tid_t recipient_tid,
    tid_t replier_tid, uint32_t subject,
    uint32_t send_flags, uint32_t recv_flags)
{
    return _send_and_receive_message(sender, recipient_tid, replier_tid, subject,
        send_flags, recv_flags, false);
}

NON_NULL_PARAMS
int attach_send_wait_queue(tcb_t* sender, tid_t recipient);

NON_NULL_PARAMS
int attach_receive_wait_queue(tcb_t* receiver, tid_t sender);

NON_NULL_PARAMS
int detach_send_wait_queue(tcb_t* sender);

NON_NULL_PARAMS
int detach_receive_wait_queue(tcb_t* receiver);

#endif /* MESSAGE */
