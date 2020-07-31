#ifndef MESSAGE_H
#define MESSAGE_H

#include <oslib.h>
#include <kernel/thread.h>
#include <kernel/struct.h>

struct Port
{
  struct ThreadControlBlock *owner;
  pid_t pid;
  queue_t senderWaitQueue;
};

typedef struct Port port_t;

typedef struct PortPair
{
  pid_t remote, local;
} port_pair_t;

#define GET_PID(port)  (port == NULL ? NULL_PID : port->pid)

port_t *getPort(pid_t pid);
port_t *createPort(void);
port_t *createPortWithPid(pid_t pid);
void releasePort(port_t *port);

HOT(int sendMessage(struct ThreadControlBlock *sender, pid_t pid, int block));
HOT(int receiveMessage(struct ThreadControlBlock *recipient, pid_t senderPid, tid_t senderTid, int block));
int sendExceptionMessage(struct ThreadControlBlock *sender, pid_t remotePid, int args[5]);

int attachSendQueue(struct ThreadControlBlock *sender, port_t *port);
int attachReceiveQueue(struct ThreadControlBlock *receiver, struct ThreadControlBlock *sender);
int detachSendQueue(struct ThreadControlBlock *sender);
int detachReceiveQueue(struct ThreadControlBlock *receiver);

extern tree_t portTree;

#endif /* MESSAGE */
