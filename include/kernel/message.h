#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>

#define MAX_PORTS   1024

struct Port
{
  tid_t owner, sendWaitTail;
};

struct PortPair
{
  pid_t remote, local;
};

struct Port portTable[MAX_PORTS];

#define getPort(pid)   ((pid == NULL_PID || pid >= MAX_PORTS) ? NULL : (&portTable[pid]))
#define GET_PID(port)  (port == NULL ? NULL_PID : (pid_t)(port - portTable))

HOT(int sendMessage(TCB *tcb, struct PortPair portPair, int block, int args[5]));
HOT(int receiveMessage(TCB *tcb, struct PortPair portPair, int block));

#endif /* MESSAGE */
