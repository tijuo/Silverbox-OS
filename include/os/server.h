#ifndef _OS_SERVER_H
#define _OS_SERVER_H

#include <oslib.h>
#include <os/message.h>

#define MAX_PROTOCOLS		8
//#define PCT			struct ProtocolCallbackTable
#define MAX_PCTS		128

struct ProtocolCallbackTable
{
  mid_t mid;
  struct MessageQueue *queue;
  void (*handlers[MAX_PROTOCOLS])(struct Message *);
};

typedef struct ProtocolCallbackTable PCT;

void server_loop(void);
int __init_pct( PCT *pct, mid_t mid, struct MessageQueue *queue );
PCT *__create_pct( mid_t mid, struct MessageQueue *queue );
int __set_protocol_handler( PCT *pct, int hnum,
                     void (*fptr)(struct Message *) );
int __get_protocol_handler( PCT *pct, int hnum,
                     void (**handler)(struct Message *) );
int detachPCT( mid_t mbox );
int attachPCT( PCT *pct );
void _init_pct_table( void );

volatile PCT pctTable[MAX_PCTS];
volatile int _num_pcts;

#endif /* _OS_SERVER_H */
