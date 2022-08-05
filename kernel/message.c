#include <kernel/mm.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>
#include <kernel/memory.h>
#include <kernel/syscall.h>
#include <kernel/message.h>
#include <kernel/error.h>
#include <kernel/bits.h>

/**
    Attach a sending thread to a recipient's send queue. The sender will then enter
    the WAIT_FOR_RECV state until the recipient receives the message from
    the thread.

    @param sender The thread to attach.
    @param recipient_tid The tid of the recipient to which the sender will be attached. (Must not be NULL_TID)
    @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int attach_send_wait_queue(tcb_t *sender, tid_t recipient_tid) {
    assert(recipient_tid != NULL_TID);

    tcb_t *recipient = get_tcb(recipient_tid);

    list_enqueue(&recipient->sender_wait_queue, sender);
    sender->thread_state = WAIT_FOR_RECV;
    sender->wait_tid = recipient_tid;

    return E_OK;
}

/**
    Attach a recipient to a sender's receive queue. The receiving thread will
    then enter the WAIT_FOR_SEND state until the sender sends a message to
    the recipient.

    @param recipient The thread that is waiting to receive a message from
    a sender.
    @param sender_tid The tid of the thread that the recipient is waiting to send a message.
    @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int attach_receive_wait_queue(tcb_t *recipient, tid_t sender_tid) {
    tcb_t *sender = get_tcb(sender_tid);

    // If sender is NULL, then the kernel sent the message. No need to attach to a wait queue

    if(sender)
        list_enqueue(&sender->receiver_wait_queue, recipient);

    recipient->thread_state = WAIT_FOR_SEND;
    recipient->wait_tid = sender_tid;

    return E_OK;
}

/**
    Remove a sender from its recipient's send queue.

    @param sender The sender to be detached. (Must not be NULL)
    @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int detach_send_wait_queue(tcb_t *sender) {
    tcb_t *recipient = get_tcb(sender->wait_tid);

    sender->wait_tid = NULL_TID;

    if(recipient)
        list_remove(&recipient->sender_wait_queue, sender);

    return E_OK;
}

/** Remove a recipient from its sender's receive queue.

 @param recipient The recipient that will be detached. (Must not be NULL)
 @return E_OK on success. E_FAIL on failure.
 */

NON_NULL_PARAMS int detach_receive_wait_queue(tcb_t *recipient) {
    tcb_t *sender = get_tcb(recipient->wait_tid);

    recipient->wait_tid = NULL_TID;

    if(sender)
        list_remove(&sender->receiver_wait_queue, recipient);

    return E_OK;
}

/**
    Synchronously send a message from a sender thread to a recipient thread.

    If the recipient is not ready to receive the message, then wait unless
    non-blocking.

    @param sender The sending thread.
    @param recipient_tid The TID of the recipient thread.
    @param replier_tid The TID of the replier from which the sender will wait for a reply
    (ignored if send_only is true.)
    @param subject The subject of the sending message.
    @param send_flags The message flags for the sent message.
    @param recv_flags The flags for the received message (ignored if send_only is true.)
    @param send_only true if the sender isn't expecting to receive a reply message.
    false if the sender expects to receive a message from a replier.
    @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
    E_BLOCK if no recipient is ready to receive (and not blocking).
 */

// XXX: What if recipient is waiting to receive a message from TID x, but
// XXX: TID x sends an exception message to recipient? Recipient's
// XXX: receive should return with E_INTERRUPT. And any subsequent
// XXX: receive()s from that TID should result in E_INTERRUPT.
NON_NULL_PARAMS
int _send_and_receive_message(tcb_t *sender, tid_t recipient_tid, tid_t replier_tid,
                              unsigned long int subject, unsigned long int send_flags,
                              unsigned long int recv_flags, bool send_only) {
    tid_t sender_tid = get_tid(sender);
    tcb_t *recipient;
    int is_kernel_message = IS_FLAG_SET(send_flags, MSG_KERNEL);

//  kprintf("sendMessage(): %d->%d subject %#x flags: %#x\n", sender_tid, msg->target.recipient, msg->subject, msg->flags);

    if(sender_tid == recipient_tid) // If sender wants to call itself, then simply set the return value
        RET_MSG(E_INVALID_ARG, "Sender attempted to send a message to itself.");
    else if(sender_tid == replier_tid)
        RET_MSG(E_INVALID_ARG,
                "Sender attempted to receive a message from itself.");
    else if(!(recipient = get_tcb(recipient_tid)))
        RET_MSG(E_INVALID_ARG, "Invalid recipient.");
    else if(recipient->thread_state == INACTIVE || recipient->thread_state == ZOMBIE)
        RET_MSG(E_UNREACH, "Attempting to send message to inactive thread.");

    // TODO: Do not allow cycles to form in wait queues.

    // If the recipient is waiting for a message from this sender or any sender

    if(recipient->thread_state == WAIT_FOR_SEND && (recipient->wait_tid
            == ANY_SENDER
            || (recipient->wait_tid == sender_tid && !is_kernel_message
                && !recipient
                ->wait_for_kernel_msg)
            || (is_kernel_message && recipient
                ->wait_for_kernel_msg))) {
        recipient->wait_for_kernel_msg = 0;

        //kprintf("%d is sending message to %d subject %#x flags: %#x\n", sender_tid, msg->recipient, msg->subject, msg->flags);

        start_thread(recipient);

        recipient->user_exec_state.rax = E_OK;
        recipient->user_exec_state.rbx = sender_tid;
        recipient->user_exec_state.rsi = subject;
        recipient->user_exec_state.rdi = send_flags;

        //kprintf("%d: Called %d with subject %d now waiting for response\n", getTid(sender), getTid(recipient), msg->subject);
        if(!send_only && IS_ERROR(receive_message(sender, replier_tid, recv_flags)))
            RET_MSG(E_FAIL, "Unable to complete call.");

        return E_OK;
    } else if(IS_FLAG_SET(send_flags, MSG_NOBLOCK)) // Recipient is not ready to receive, but we're not allowed to block, so return
        return E_BLOCK;
    else { // Wait until the recipient is ready to receive the message
        //kprintf("%d: Waiting to send subj: %d to %d\n", sender_tid, msg->subject, msg->recipient);

        if(IS_ERROR(remove_thread_from_list(sender)))
            RET_MSG(E_FAIL, "Unable to detach sender from run queue.");

        attach_send_wait_queue(sender, get_tid(recipient));

        //kprintf("Waiting until recipient is read...\n");

        sender->wait_for_kernel_msg = !!is_kernel_message;
        sender->user_exec_state.rax = E_INTERRUPT;

        // todo: Set a flag so that on receive(), it completes the sendAndReceive
        switch_context(schedule(get_current_processor()));

        // Does not return
    }

    RET_MSG(E_FAIL, "This should never happen.");
}

/**
    Synchronously receive a message from a thread. If no message can be received, then wait
    for a message from the sender unless indicated as non-blocking.

    @param recipient The recipient of the message.
    @param sender_tid The tid of the sender that the recipient expects to receive a message.
    ANY_SENDER, if receiving from any sender.
    @param block Non-zero if a blocking receive (i.e. wait until a sender
    sends). Otherwise, give up if no senders are available.
    @return E_OK on success. E_FAIL on failure. E_INVALID_ARG on bad argument.
    E_BLOCK if no messages are pending to be received (and non-blocking).
 */

NON_NULL_PARAMS
int receive_message(tcb_t *recipient, tid_t sender_tid, unsigned long int flags) {
    tcb_t *sender = get_tcb(sender_tid);
    tid_t recipient_tid = get_tid(recipient);
    int is_kernel_message = IS_FLAG_SET(flags, MSG_KERNEL);

    //kprintf("%d: receiveMessage()\n", recipient_tid);

    // TODO: Do not allow cycles to form in wait queues.

    if(recipient == sender)
        RET_MSG(E_INVALID_ARG,
                "Recipient attempted to receive message from itself.");
    else if(sender
            && (sender->thread_state == INACTIVE || sender->thread_state == ZOMBIE))
        RET_MSG(E_UNREACH, "Attempting to receive message from inactive thread.");

    if(!sender && !is_list_empty(&recipient->sender_wait_queue)) { // receive message from anyone
        sender = list_dequeue(&recipient->sender_wait_queue);
        sender_tid = get_tid(sender);
    }

    //kprintf("receiveMessage(): %d<-%d flags: %#x\n", recipient_tid, sender_tid, msg->flags);

    if(sender && sender->wait_tid == recipient_tid
            && (!is_kernel_message || (is_kernel_message && sender->wait_for_kernel_msg))) {
        //kprintf("%d: Receiving message...\n", recipient_tid);

        start_thread(sender);

        sender->wait_for_kernel_msg = 0;

        sender->user_exec_state.rax = E_OK;
        recipient->user_exec_state.rax = E_OK;
        recipient->user_exec_state.rbx = sender_tid;
        recipient->user_exec_state.rsi = sender->user_exec_state.rsi; // sender flags
        recipient->user_exec_state.rdi = sender->user_exec_state.rdi;

        //kprintf("%d is receiving message from %d subject %#x flags: %#x\n", recipient_tid, sender_tid, msg->subject, msg->flags);

        switch_context(recipient); // don't use sysexit. do an iret instead so that args are restored
        // Does not return
    } else if(IS_FLAG_SET(flags, MSG_NOBLOCK))
        return E_BLOCK;
    else { // no one is waiting to send to this local port, so wait
        //kprintf("%d: Waiting to receive from %d\n", recipient_tid, msg->sender);

        if(IS_ERROR(remove_thread_from_list(recipient)))
            RET_MSG(E_FAIL, "Unable to detach recipient from run queue.");

        attach_receive_wait_queue(recipient, sender_tid);

        recipient->wait_for_kernel_msg = !!is_kernel_message;
        recipient->user_exec_state.rax = E_INTERRUPT;

        // Receive will be completed when sender does a send

        switch_context(schedule(get_current_processor()));
    }

    RET_MSG(E_FAIL, "This should never happen.");
}
