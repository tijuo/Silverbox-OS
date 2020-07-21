#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/struct.h>

typedef struct Port
{
  tid_t owner, sendWaitTail;
  pid_t pid;
} port_t;

typedef struct PortPair
{
  pid_t remote, local;
} port_pair_t;

#define GET_PID(port)  (port == NULL ? NULL_PID : port->pid)

port_t *getPort(pid_t pid);
port_t *createPort(void);

HOT(int sendMessage(tcb_t *tcb, port_pair_t portPair, int block, int args[5]));
HOT(int receiveMessage(tcb_t *tcb, port_pair_t portPair, int block));

void attachSendQueue(tcb_t *tcb, pid_t pid);
void attachReceiveQueue(tcb_t *tcb, pid_t pid);
void detachSendQueue(tcb_t *tcb, pid_t pid);
void detachReceiveQueue(tcb_t *tcb, pid_t pid);

extern tree_t portTree;

#endif /* MESSAGE */
