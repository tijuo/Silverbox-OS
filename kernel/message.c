#include <kernel/bits.h>
#include <kernel/debug.h>
#include <kernel/error.h>
#include <kernel/memory.h>
#include <kernel/message.h>
#include <kernel/mm.h>
#include <kernel/schedule.h>
#include <kernel/thread.h>
#include <os/syscalls.h>

/** Attach a sending thread to a recipient's send queue. The sender will then enter
 the WAIT_FOR_RECV state until the recipient receives the message from
 the thread.

 @param sender The thread to attach.
 @param recipient The recipient to which the sender will be attached. If `NULL`, then
 just put the sender into `WAIT_FOR_RECV` status.
 */
NON_NULL_PARAM(1) void attach_sender_wait_queue(tcb_t* sender, tcb_t *recipient)
{
    if(recipient) {
        list_enqueue(&recipient->sender_wait_queue, sender);
    }

    sender->thread_state = WAIT_FOR_RECV;
    sender->wait_tid = get_tid(recipient);
}

/** Attach a recipient to a sender's receive queue. The receiving thread will
 then enter the WAIT_FOR_SEND state until the sender sends a message to
 the recipient.

 @param recipient The thread that is waiting to receive a message from
 a sender.
 @param sender The thread that the recipient is waiting to send a message. If `NULL`,
 then just put the recipient into `WAIT_FOR_SEND` status.
 */
NON_NULL_PARAM(1) void attach_receiver_wait_queue(tcb_t* recipient, tcb_t *sender)
{
    // If sender is NULL, then the kernel sent the message. No need to attach to a wait queue

    if(sender) {
        list_enqueue(&sender->receiver_wait_queue, recipient);
    }

    recipient->thread_state = WAIT_FOR_SEND;
    recipient->wait_tid = get_tid(sender);
}

/** Remove a sender from its recipient's sender queue.

 @param sender The sender to be detached. Does nothing if the sender
 isn't already in a sender queue.
 */
NON_NULL_PARAMS void detach_sender_wait_queue(tcb_t* sender)
{
    tcb_t* recipient = get_tcb(sender->wait_tid);
    sender->wait_tid = NULL_TID;

    if(recipient) {
        list_remove(&recipient->sender_wait_queue, sender);
    }
}

/** Remove a recipient from its sender's receiver queue.

 @param recipient The recipient to be detached. Does nothing if the recipient
 isn't already in a receiver queue.
 */
NON_NULL_PARAMS void detach_receiver_wait_queue(tcb_t* recipient)
{
    tcb_t* sender = get_tcb(recipient->wait_tid);
    recipient->wait_tid = NULL_TID;

    if(sender) {
        list_remove(&sender->receiver_wait_queue, recipient);
    }
}

/**
    Send a message to a recipient.
    
    @param sender - The thread sending the message.
    @param recipient_tid - The TID of the thread receiving the message. If `ANY_RECEIVER`, then send
    the message to any thread that wishes to receive (or block if none are available).
    @param flags - Message flags
    @param subject - Metadata describing to the contents of the message.
    @param buffer - The send buffer. If `NULL`, then only the message header will be sent.
    @param buffer_length - The length of the send buffer. If 0, then only the message header will be sent.
    @param message - The message to be sent to a receiver.  If `MSG_NOBLOCK` is set in `flags`
    then immediately return `E_BLOCK` if the send would block.

    @return
        * E_OK upon success.
        * E_FAIL upon failure.
        * E_INVALID_ARG, if an argument is bad.
        * E_BLOCK, if the sender isn't ready to send and the `MSG_NOBLOCK` flag is set.
        * E_PREEMPT, if a kernel message has been received when the `MSG_KERNEL` flag isn't set.
        * E_INTERRUPT, if the kernel unexpectedly wakes up the thread while waiting for transmission.
        * E_UNREACH, if the recipient doesn't exist or isn't active.
*/
WARN_UNUSED NON_NULL_PARAM(1) int send_message(tcb_t *sender, tid_t recipient_tid, uint32_t subject, uint16_t flags, void *send_buffer, size_t send_buffer_length) {
    /*
    # Register layout

    ## Waiting to transfer

    ### Sender

    * `eax` - `E_INTERRUPT` (return value for the system call if a thread is prematurely awoken)

    * `ebx` - `buffer`

    * `ecx` - `flags`

    * `esi` - `subject`

    * `edi` - `length`
    
    ## Finished transfer
    
    ### Recipient

    * `eax` - return value: `E_OK`, `E_PREEMPT`, `E_FAIL`

    * `ebx` - `flags` (upper 16 bits), `sender_tid` (lower 16 bits)

    * `esi` - `subject`

    * `edi` - `sent_bytes` (number of bytes the recipient has received)
    
    ### Sender

    * `ebx` - `recipient_tid`

    * `esi` - `sent_bytes`
    */
    tid_t sender_tid = get_tid(sender);
    tcb_t* recipient = get_tcb(recipient_tid);
    size_t actually_sent = 0;

    bool is_transfer_kernel_msg = IS_FLAG_SET(flags, MSG_KERNEL);

    //kprintf("sendMessage(): %d->%d subject %#x flags: %#x\n", sender_tid, recipient_tid, subject, flags);

    if(sender_tid == recipient_tid) {
        RET_MSG(E_INVALID_ARG, "Sender attempted to send a message to itself.");
    } else if(send_buffer_length > 0 && send_buffer == NULL) {
        RET_MSG(E_INVALID_ARG, "Send buffer is NULL, but send length is non-zero.");
    }

    if(recipient_tid == ANY_RECIPIENT && !LIST_IS_EMPTY(&sender->receiver_wait_queue)) {
        recipient = list_dequeue(&sender->receiver_wait_queue);
        recipient_tid = get_tid(recipient);

        KASSERT(recipient->thread_state == WAIT_FOR_SEND);
    }

    if(recipient && (recipient->thread_state == INACTIVE || recipient->thread_state == ZOMBIE)) {
        RET_MSG(E_UNREACH, "Recipient is not an active thread.");
    }

    // TODO: Do not allow cycles to form in wait queues.

    // Only send messages if the recipient is waiting to receive, and:
    // 1. a kernel message is being sent, or
    // 2. the recipient is waiting for a message from the sender
    // 3. the recipient is waiting for a message from anyone (and isn't ignoring non-kernel messages)

    // Kernel messages always have priority over other messages

    // If the recipient is waiting for a message from this sender or any sender

    if(recipient && recipient->thread_state == WAIT_FOR_SEND && (is_transfer_kernel_msg || (recipient->wait_tid == ANY_SENDER && !recipient->wait_for_kernel_msg) || recipient->wait_tid == sender_tid)) {
        KASSERT(!recipient->wait_for_kernel_msg || recipient->wait_tid == NULL_TID);

        addr_t recipient_buffer = (addr_t)recipient->user_exec_state.ebx; // Contains the recv buffer (set by this function during the receive)
        size_t recipient_buffer_length = (size_t)recipient->user_exec_state.edi; // Contains the recv buffer count (also set during the receive)

        actually_sent = MIN(recipient_buffer_length, send_buffer_length);

        if(IS_ERROR(poke_virt((addr_t)recipient_buffer, actually_sent, send_buffer, recipient->root_pmap))) {
            RET_MSG(E_FAIL, "Unable to write message to recipient's receive buffer.");
        }

        if(IS_ERROR(thread_start(recipient))) {
            RET_MSG(E_FAIL, "Unable to awake waiting recipient.");
        }

        recipient->user_exec_state.eax = (uint32_t)((is_transfer_kernel_msg && !recipient->wait_for_kernel_msg) ? E_PREEMPT : E_OK);
        recipient->user_exec_state.ebx = (uint32_t)(is_transfer_kernel_msg ? KERNEL_TID : sender_tid) | ((uint32_t)flags << 16);
        recipient->user_exec_state.esi = subject;
        recipient->user_exec_state.edi = (uint32_t)actually_sent;

        recipient->wait_for_kernel_msg = 0;

        sender->user_exec_state.ebx = (uint32_t)recipient_tid;
        sender->user_exec_state.esi = (uint32_t)actually_sent;
        return E_OK;
    } else if(IS_FLAG_SET(flags, MSG_NOBLOCK)) { // Recipient is not ready to receive, but we're not allowed to block, so return
        return E_BLOCK;
    } else // Wait until the recipient is ready to receive the message
    {
        if(IS_ERROR(thread_remove_from_list(sender))) {
            RET_MSG(E_FAIL, "Unable to detach sender from run queue.");
        }

        attach_sender_wait_queue(sender, recipient);

        sender->wait_for_kernel_msg = (uint32_t)is_transfer_kernel_msg;
        
        sender->user_exec_state.eax = (uint32_t)E_INTERRUPT;
        sender->user_exec_state.ebx = (uint32_t)send_buffer; // Set the send buffer so that the receiver can receive the data in a later call
        sender->user_exec_state.ecx = (uint32_t)flags;
        sender->user_exec_state.esi = (uint32_t)subject;
        sender->user_exec_state.edi = (uint32_t)send_buffer_length;

        thread_switch_context(schedule(processor_get_current()), true);

        // Does not return
        UNREACHABLE;
    }

    // Just in case it returns anyway...
    return E_FAIL;
}

/**
    Receive a message from a sender.
    
    @param recipient - The thread receiving the message.
    @param sender_tid - The TID of the thread that will be sending a message. If `ANY_SENDER`, then receive
    the message from any thread that wishes to send (or block if none are available).
    @param flags - Message flags. `MSG_KERNEL` - only receive kernel messages and ignore all other messages.
    `MSG_NOBLOCK` - Do not wait for the sender to send a message, if they're not ready.
    @param buffer - The receive buffer. If `NULL`, then only the message header will be sent.
    @param buffer_length - The maximum number of bytes to receive in the buffer. If `buffer_length` is 0, then only receive the
    message header.

    @return
        * `E_OK` upon success.

        * `E_FAIL` upon failure.

        * `E_INVALID_ARG`, if an argument is bad.

        * `E_BLOCK`, if the sender isn't ready to send and the `MSG_NOBLOCK` flag is set.

        * `E_PREEMPT`, if a kernel message has been received when the `MSG_KERNEL` flag isn't set.

        * `E_INTERRUPT`, if the kernel unexpectedly wakes up the thread while waiting for transmission.
*/
WARN_UNUSED NON_NULL_PARAM(1) int receive_message(tcb_t *recipient, tid_t sender_tid, uint16_t flags, void *recv_buffer, size_t recv_buffer_length) {
    /*
    # Register layout

    ## Waiting to transfer

    ### Recipient

    * `eax` - `E_INTERRUPT` (return value for the system call if a thread is prematurely awoken)

    * `ebx` - `buffer`

    * `ecx` - `flags`
    
    * `edi` - `length`
    
    ## Finished transfer
    
    ### Recipient

    * `eax` - return value: `E_OK`, `E_PREEMPT`, `E_FAIL`

    * `ebx` - `flags` (upper 16 bits), `sender_tid` (lower 16 bits)

    * `esi` - `subject`

    * `edi` - `rcvd_bytes`
    
    ### Sender

    * `eax` - return value: `E_OK`, `E_FAIL`, `E_UNREACH`, `E_INVALID_ARG`, `E_BLOCK`

    * `esi` - `rcvd_bytes` (number of bytes the sender has sent)
    */
    tid_t recipient_tid = get_tid(recipient);
    tcb_t* sender = get_tcb(sender_tid);
    bool is_expecting_kernel_msg = IS_FLAG_SET(flags, MSG_KERNEL);
    size_t actually_recv = 0;

    if(recipient_tid == sender_tid) {
        RET_MSG(E_INVALID_ARG, "Recipient attempted to receive a message from itself.");
    } else if(recv_buffer_length > 0 && recv_buffer == NULL) {
        RET_MSG(E_INVALID_ARG, "Receive buffer is NULL, but receive length is non-zero.");
    }

    if(sender_tid == ANY_SENDER && recv_buffer_length > 0 && !LIST_IS_EMPTY(&recipient->sender_wait_queue)) {
        // Receive a message from anyone

        sender = list_dequeue(&recipient->sender_wait_queue);
        sender_tid = get_tid(sender);
    }

    // Now have the sender wait for the reply from the recipient (unless blocking)

    // kprintf("%d: receiveMessage()\n", recipient_tid);

    // TODO: Do not allow cycles to form in wait queues.

    // Kernel messages always have priority over other messages

    //is_expecting_kernel_msg = recipient->wait_for_kernel_msg;

    // Only accept messages if the sender is waiting to send, and:
    // 1. a kernel message is being sent, or
    // 2. we're waiting on a message from the sender
    // 3. we're waiting on a message from any sender (and we aren't ignoring non-kernel messages)

    if(sender && sender->thread_state == WAIT_FOR_RECV && (sender->wait_for_kernel_msg || sender->wait_tid == recipient_tid || (!is_expecting_kernel_msg && sender->wait_tid == ANY_RECIPIENT))) {
        // kprintf("%d: Receiving message...\n", recipient_tid);

        addr_t send_buffer = (addr_t)sender->user_exec_state.ebx; // Contains the send buffer (set by this function during the send)
        size_t send_buffer_length = (size_t)sender->user_exec_state.edi; // Contains the send buffer count (also set during the send)

        actually_recv = MIN(send_buffer_length, recv_buffer_length);

        if(IS_ERROR(peek_virt((addr_t)send_buffer, actually_recv, recv_buffer, sender->root_pmap))) {
            RET_MSG(E_FAIL, "Unable to read message from sender's send buffer.");
        }

        if(IS_ERROR(thread_start(sender))) {
            RET_MSG(E_FAIL, "Unable to awake awaiting sender.");
        }

        bool did_send_kernel_msg = !!(sender->wait_for_kernel_msg);

        // Transfer the data between buffers and update the number of bytes sent/received
        // No need to set EAX, we return the status directly

        recipient->user_exec_state.ebx = (uint32_t)(sender->wait_for_kernel_msg ? KERNEL_TID : sender_tid) | (sender->user_exec_state.ecx << 16);
        recipient->user_exec_state.esi = sender->user_exec_state.esi;
        recipient->user_exec_state.edi = actually_recv;

        sender->wait_for_kernel_msg = 0;
        sender->user_exec_state.eax = E_OK;
        sender->user_exec_state.esi = actually_recv;

        return (is_expecting_kernel_msg && !did_send_kernel_msg) ? E_PREEMPT : E_OK;
    } else if(IS_FLAG_SET(flags, MSG_NOBLOCK)) {
        return E_BLOCK;
    } else // no one is waiting to send to this local port, so wait
    {
        // kprintf("%d: Waiting to receive from %d\n", recipient_tid, msg->sender);

        if(IS_ERROR(thread_remove_from_list(sender)))
            RET_MSG(E_FAIL, "Unable to detach recipient from run queue.");

        attach_receiver_wait_queue(sender, recipient);

        recipient->wait_for_kernel_msg = is_expecting_kernel_msg;

        recipient->user_exec_state.eax = (uint32_t)E_INTERRUPT;
        recipient->user_exec_state.ebx = (uint32_t)recv_buffer; // Set the send buffer so that the receiver can receive the data in a later call
        recipient->user_exec_state.ecx = (uint32_t)flags;
        recipient->user_exec_state.edi = (uint32_t)recv_buffer_length;
        // Receive will be completed when sender does a send

        thread_switch_context(schedule(processor_get_current()), true);

        // Does not return
        UNREACHABLE;
    }

    // Just in case it returns anyway...
    return E_FAIL;
}
