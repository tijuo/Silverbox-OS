#include <kernel/bits.h>
#include <kernel/debug.h>
#include <kernel/error.h>
#include <kernel/memory.h>
#include <kernel/message.h>
#include <kernel/mm.h>
#include <kernel/schedule.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>
#include <os/syscalls.h>

/** Attach a sending thread to a recipient's send queue. The sender will then enter
 the WAIT_FOR_RECV state until the recipient receives the message from
 the thread.

 @param sender The thread to attach.
 @param recipient_tid The tid of the recipient to which the sender will be attached. (Must not be NULL_TID)
 @return E_OK on success. E_FAIL on failure.
 */
NON_NULL_PARAMS int attach_send_wait_queue(tcb_t* sender, tid_t recipient_tid)
{
    KASSERT(recipient_tid != NULL_TID);

    tcb_t* recipient = get_tcb(recipient_tid);

    list_enqueue(&recipient->sender_wait_queue, sender);
    sender->thread_state = WAIT_FOR_RECV;
    sender->wait_tid = recipient_tid;

    return E_OK;
}

/** Attach a recipient to a sender's receive queue. The receiving thread will
 then enter the WAIT_FOR_SEND state until the sender sends a message to
 the recipient.

 @param recipient The thread that is waiting to receive a message from
 a sender.
 @paarm sender_tid The tid of the thread that the recipient is waiting to send a message.
 @return E_OK on success. E_FAIL on failure.
 */
NON_NULL_PARAMS int attach_receive_wait_queue(tcb_t* recipient, tid_t sender_tid)
{
    tcb_t* sender = get_tcb(sender_tid);

    // If sender is NULL, then the kernel sent the message. No need to attach to a wait queue

    if(sender)
        list_enqueue(&sender->receiver_wait_queue, recipient);

    recipient->thread_state = WAIT_FOR_SEND;
    recipient->wait_tid = sender_tid;

    return E_OK;
}

/** Remove a sender from its recipient's send queue.

 @param sender The sender to be detached. (Must not be NULL)
 @return E_OK on success. E_FAIL on failure.
 */
NON_NULL_PARAMS int detach_send_wait_queue(tcb_t* sender)
{
    tcb_t* recipient = get_tcb(sender->wait_tid);

    sender->wait_tid = NULL_TID;

    if(recipient)
        list_remove(&recipient->sender_wait_queue, sender);

    return E_OK;
}

/** Remove a recipient from its sender's receive queue.

 @param recipient The recipient that will be detached. (Must not be NULL)
 @return E_OK on success. E_FAIL on failure.
 */
NON_NULL_PARAMS int detach_receive_wait_queue(tcb_t* recipient)
{
    tcb_t* sender = get_tcb(recipient->wait_tid);

    recipient->wait_tid = NULL_TID;

    if(sender)
        list_remove(&sender->receiver_wait_queue, recipient);

    return E_OK;
}

/**
 Synchronously send a message to a recipient thread and wait for a response from a replying thread.

 If the recipient is not ready to receive the message or the replier is not ready to reply, then wait unless
 non-blocking.

 @param sender The sending thread.
 @param recipient_tid The TID of the recipient thread.
 @param subject The subject of the sending message.
 @param flags The message flags for the sent and received messages.
 @param args The message payload data. The buffer used for sending data must be at least
 `send_length` long. The buffer used for receiving data must be at least `recv_length` long. A `send_length`
 of 0 implies that no data needs to be sent (receive-only). A `recv_length` of 0 implies no data needs to be
 received (send-only).
 @return
    * E_OK upon success.
    * E_FAIL upon failure.
    * E_INVALID_ARG, if an argument is bad.
    * E_BLOCK, if the recipient isn't ready to receive and the `MSG_NOBLOCK` flag is set.
    * E_UNREACH, if the recipient doesn't exist or isn't active.
    * E_PREEMPT, if a kernel message has been received when the `MSG_KERNEL` flag isn't set.
    * E_INTERRUPT, if the kernel unexpectedly wakes up the thread while waiting for transmission.
 */
NON_NULL_PARAMS
int send_and_receive_message(tcb_t* sender, tid_t recipient_tid,
    uint32_t subject, uint16_t flags,
    SysMessageArgs* args)
{
    // TODO: There should be the capability for a sender to send a message to anyone who is listening.

    /*
        If a sender or receiver blocks, registers EBX and EDI will be used to store the buffer and transfer lengths, respectively
        until the other party is ready.

        Upon succesful message transfer, the registers will contain:
            * EAX - status code
            * EBX - lower 16 bits: sender TID (if receiving), receiver TID (if sending). upper 16 bits: message flags.
            * ESI - the number of bytes sent.
            * EDI - the number of bytes received.
       */

    tid_t sender_tid = get_tid(sender);
    tcb_t* recipient = get_tcb(recipient_tid);
    size_t actually_sent = 0;
    size_t actually_recv = 0;

    bool is_transfer_kernel_msg = IS_FLAG_SET(flags, MSG_KERNEL);

    if(is_transfer_kernel_msg && args->send_length != 0)
        RET_MSG(E_INVALID_ARG, "Sending messages to the kernel is not (yet) supported.");

    //  kprintf("sendMessage(): %d->%d subject %#x flags: %#x\n", sender_tid, msg->target.recipient, msg->subject, msg->flags);

    if(sender_tid == recipient_tid)
        RET_MSG(E_INVALID_ARG, "Sender attempted to send a message to itself.");
    else if(args->send_length == 0 && args->recv_length == 0)
        RET_MSG(E_INVALID_ARG, "Send length and receive length are both 0.");
    else if(args->send_length > 0 && args->send_buffer != NULL)
        RET_MSG(E_INVALID_ARG, "Send buffer is NULL, but send length is non-zero.");
    else if(args->recv_length > 0 && args->recv_buffer != NULL)
        RET_MSG(E_INVALID_ARG, "Receive buffer is NULL, but receive length is non-zero.");

    if(recipient_tid == NULL_TID && args->send_length > 0 && !LIST_IS_EMPTY(&sender->receiver_wait_queue)) {
        recipient = list_dequeue(&sender->receiver_wait_queue);
        recipient_tid = get_tid(recipient);

        KASSERT(recipient->thread_state == WAIT_FOR_SEND);
    }

    if(recipient && (recipient->thread_state == INACTIVE || recipient->thread_state == ZOMBIE))
        RET_MSG(E_UNREACH, "Recipient is not an active thread.");

    // TODO: Do not allow cycles to form in wait queues.

    // If the recipient is waiting for a message from this sender or any sender

    if(recipient && args->send_length > 0) {
        KASSERT(!recipient->wait_for_kernel_msg || recipient->wait_tid == NULL_TID);

        // Kernel messages always have priority over other messages

        // Only send messages if the recipient is waiting to receive, and:
        // 1. a kernel message is being sent, or
        // 2. the recipient is waiting for a message from the sender
        // 3. the recipient is waiting for a message from anyone (and isn't ignoring non-kernel messages)

        if(recipient->thread_state == WAIT_FOR_SEND && (is_transfer_kernel_msg || (recipient->wait_tid == ANY_SENDER && !recipient->wait_for_kernel_msg) || recipient->wait_tid == sender_tid)) {
            addr_t recipient_buffer = (addr_t)recipient->user_exec_state.ebx; // Contains the recv buffer (set by this function during the receive)
            size_t recipient_buffer_len = (size_t)recipient->user_exec_state.edi; // Contains the recv buffer count (also set during the receive)

            actually_sent = recipient_buffer_len < args->send_length ? recipient_buffer_len : args->send_length;

            if(IS_ERROR(poke_virt((addr_t)recipient_buffer, recipient_buffer_len, args->send_buffer, recipient->root_pmap))) {
                RET_MSG(E_FAIL, "Unable to write message to recipient's receive buffer.");
            }

            if(IS_ERROR(start_thread(recipient))) {
                RET_MSG(E_FAIL, "Unable to awake waiting recipient.");
            }

            recipient->user_exec_state.eax = (is_transfer_kernel_msg && !recipient->wait_for_kernel_msg) ? E_PREEMPT : E_OK;
            recipient->user_exec_state.ebx = (uint32_t)(is_transfer_kernel_msg ? KERNEL_TID : sender_tid) | ((uint32_t)flags << 16);
            recipient->user_exec_state.ecx = subject;
            recipient->user_exec_state.edi = actually_sent;

            recipient->wait_for_kernel_msg = 0;
        } else if(IS_FLAG_SET(flags, MSG_NOBLOCK)) // Recipient is not ready to receive, but we're not allowed to block, so return
            return E_BLOCK;
        else // Wait until the recipient is ready to receive the message
        {
            if(IS_ERROR(remove_thread_from_list(sender)))
                RET_MSG(E_FAIL, "Unable to detach sender from run queue.");

            if(IS_ERROR(attach_send_wait_queue(sender, get_tid(recipient)))) {
                if(IS_ERROR(start_thread(sender))) {
                    panic("The sender couldn't be attached to the recipient's sender wait queue, but it also failed to restart.");
                }
            }

            sender->wait_for_kernel_msg = (uint32_t)is_transfer_kernel_msg;

            sender->user_exec_state.eax = (uint32_t)E_INTERRUPT;
            sender->user_exec_state.ebx = (uint32_t)args->send_buffer; // Set the send buffer so that the receiver can receive the data in a later call
            sender->user_exec_state.edi = (uint32_t)args->send_length;

            switch_context(schedule(get_current_processor()), 1);

            // Does not return
        }
    }

    sender->user_exec_state.esi = (uint32_t)actually_sent;
    sender->user_exec_state.ebx = (uint32_t)(args->recv_buffer);
    sender->user_exec_state.edi = (uint32_t)args->recv_length;

    // Receive code

    // Note that from here on after, "recipient" is actually the sender and *we're* the recipient.

    if(recipient_tid == ANY_SENDER && args->recv_length > 0 && !LIST_IS_EMPTY(&sender->sender_wait_queue)) {
        // Receive a message from anyone

        recipient = list_dequeue(&sender->sender_wait_queue);
        recipient_tid = get_tid(recipient);
    }

    if(recipient) {
        if(args->recv_length > 0) {
            // Now have the sender wait for the reply from the recipient (unless blocking)

            //if(IS_ERROR(receive_message(sender, replier_tid, flags)))
            //    RET_MSG(E_FAIL, "Unable to complete call.");

            {
                // kprintf("%d: receiveMessage()\n", recipient_tid);

                // TODO: Do not allow cycles to form in wait queues.

                // Kernel messages always have priority over other messages

                //is_transfer_kernel_msg = recipient->wait_for_kernel_msg;

                // Only accept messages if the sender is waiting to send, and:
                // 1. a kernel message is being sent, or
                // 2. we're waiting on a message from the sender
                // 3. we're waiting on a message from any sender (and we aren't ignoring non-kernel messages)

                if(recipient->thread_state == WAIT_FOR_RECV && (recipient->wait_for_kernel_msg || recipient->wait_tid == sender_tid || (!is_transfer_kernel_msg && recipient->wait_tid == ANY_RECIPIENT))) {
                    // kprintf("%d: Receiving message...\n", recipient_tid);

                    addr_t buffer = (addr_t)recipient->user_exec_state.ebx; // Contains the send buffer (set by this function during the send)
                    size_t buffer_len = (size_t)recipient->user_exec_state.edi; // Contains the send buffer count (also set during the send)

                    actually_recv = buffer_len < args->recv_length ? buffer_len : args->recv_length;

                    if(IS_ERROR(peek_virt((addr_t)buffer, buffer_len, args->recv_buffer, recipient->root_pmap))) {
                        RET_MSG(E_FAIL, "Unable to read message from sender's send buffer.");
                    }

                    if(IS_ERROR(start_thread(recipient))) {
                        RET_MSG(E_FAIL, "Unable to awake awaiting sender.");
                    }

                    recipient->wait_for_kernel_msg = 0;
                    recipient->user_exec_state.eax = E_OK;

                    // Transfer the data between buffers and update the number of bytes send/received
                    // No need to set EAX, we return the status directly

                    recipient->user_exec_state.edi = actually_sent;

                    sender->user_exec_state.ebx = (uint32_t)(is_transfer_kernel_msg ? KERNEL_TID : recipient_tid) | ((uint32_t)recipient->user_exec_state.esi << 16); // sender + flags
                    sender->user_exec_state.ecx = recipient->user_exec_state.ecx; // subject
                    sender->user_exec_state.edi = actually_recv;

                    recipient->user_exec_state.esi = actually_recv;

                    return (is_transfer_kernel_msg && !IS_FLAG_SET(flags, MSG_KERNEL)) ? E_PREEMPT : E_OK;
                } else if(IS_FLAG_SET(flags, MSG_NOBLOCK))
                    return E_BLOCK;
                else // no one is waiting to send to this local port, so wait
                {
                    // kprintf("%d: Waiting to receive from %d\n", recipient_tid, msg->sender);

                    if(IS_ERROR(remove_thread_from_list(sender)))
                        RET_MSG(E_FAIL, "Unable to detach recipient from run queue.");

                    if(IS_ERROR(attach_receive_wait_queue(sender, recipient_tid))) {
                        if(IS_ERROR(start_thread(sender))) {
                            panic("The recipient couldn't be attached to the sender's receiver wait queue, but it also failed to restart.");
                        }
                    }

                    sender->wait_for_kernel_msg = is_transfer_kernel_msg;
                    sender->user_exec_state.eax = (uint32_t)E_INTERRUPT;

                    // Receive will be completed when sender does a send

                    switch_context(schedule(get_current_processor()), 1);
                }

                RET_MSG(E_FAIL, "This should never happen.");
            }
        }
    }

    return E_OK;
}
