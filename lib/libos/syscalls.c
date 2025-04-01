#include <os/syscalls.h>

int sys_send(Message* message, size_t* bytes_sent)
{
    int ret_val;
    int dummy;
    size_t num_sent;
    uint32_t pair;

    asm volatile(SYSENTER_INSTR
        : "=a"(ret_val), "=b"(pair), "=S"(num_sent), "=d"(dummy)
        : "a"(SYS_SEND), "b"(((uint32_t)message->target.recipient) | ((uint32_t)message->flags) << 16), 
        "d"((int)message->subject), "S"((int)message->buffer), "D"((int)message->buffer_length)
        : "memory", "ecx");

    if(bytes_sent) {
        *bytes_sent = num_sent;
    }

    message->target.recipient = (tid_t)(pair >> 16);

    return ret_val;
}

int sys_receive(Message* message, size_t* bytes_rcvd)
{
    int ret_val;
    int dummy;
    size_t num_rcvd;
    uint32_t pair;
    uint32_t subject;

    asm volatile(SYSENTER_INSTR
        : "=a"(ret_val), "=b"(pair), "=c"(subject), "=d"(dummy), "=D"(num_rcvd)
        : "a"(SYS_RECEIVE), "b"(((uint32_t)message->target.sender) | ((uint32_t)message->flags) << 16), 
        "d"((int)message->buffer), "S"((int)message->buffer_length)
        : "memory");

    if(bytes_rcvd) {
        *bytes_rcvd = num_rcvd;
    }

    message->flags = (uint16_t)(pair >> 16);
    message->target.sender = (uint16_t)pair;
    message->subject = (uint32_t)subject;

    return ret_val;
}

int sys_yield(void)
{
    return sys_sleep(0, SL_SECONDS);
}

int sys_wait(void)
{
    return sys_sleep(SL_INF_DURATION, SL_SECONDS);
}

int sys_event_poll(int mask)
{
    SysPollArgs args = {
        .mask= (uint32_t)mask,
        .blocking = 0
    };
    
    return sys_read(RES_INT, &args);
}

int sys_event_eoi(int mask)
{
    SysEoiArgs args = {
        .mask = (uint32_t)mask
    };

    return sys_update(RES_INT, &args);
}

noreturn void sys_exit(int status)
{
    __asm__("hlt\n" :: "a"(status));
}

SYSCALL2(create, SysResource, res_type, void*, args);
SYSCALL2(read, SysResource, res_type, void*, args);
SYSCALL2(update, SysResource, res_type, void*, args);
SYSCALL2(destroy, SysResource, res_type, void*, args);
SYSCALL2(sleep, unsigned int, duration, SysSleepGranularity, granularity);