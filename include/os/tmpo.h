#ifndef TMPO_H
#define TMPO_H

#include <oslib.h>
#include <os/message.h>
#include <os/adt/cqueue.h>

#define TMPO_FIN	0x01
#define TMPO_SYN	0x02
#define TMPO_RST	0x04
#define TMPO_PSH	0x08
#define TMPO_ACK	0x10
#define TMPO_URG	0x20
#define TMPO_ECE	0x40
#define TMPO_CWR	0x80

enum TMPO_States { CLOSED, LISTEN, SYN_RECEIVED, SYN_SENT, ESTABLISHED,
 		   FIN_WAIT1, FIN_WAIT2, TIME_WAIT, CLOSING, CLOSE_WAIT,
                   LAST_ACK };

struct TMPO_BufMessage
{
  unsigned short seq;
  size_t len; // length of the data
  unsigned char valid; // Indicates whether a message is ready to be sent or 
                       // was received according to the sequence number
  unsigned char buffer[MAX_MSG_LEN-sizeof(struct TMPO_Header)];
};

/* Sequences are incremented by one */

/* When data are sent, (using send()/tmpo_send()) the data are placed
   in the data buffer until a full message can be formed from the
   data. When this happens, the data is transferred from the buffer
   to a TMPO_BufMessage (and corresponding fields are updated 
   [message becomes valid, data_head increased, msg_tail increased]).
   If msg_buf is full, then no new messages may be buffered. If
   data_buf is full, then send()/tmpo_send() fails. TMPO_BufMessages
   may be removed only if an acknowlegement is received for the message
   if(between_incl(recv_ack, msg.seq, msg_tail.seq, USHRT_MAX)). Invalid
   acknowledgements are discarded.
*/

struct TMPO_SendBuffer
{
  unsigned short next_seq;
  struct CQueue *msg_queue, *sbuffer;
};

/* 
   When messages are received, the messages are placed in the message 
   buffer(message becomes valid, seq updated, length set) until a 
   segment can be reassembled (according to sequence, starting from 
   next_seq). When this happens, the data is transferred from the 
   message to the data_buffer (and corresponding fields are updated 
   [data_tail increased, next_seq updated]). If msg_buf is full, 
   then no more messages may be received(lost). If data_buf is full, 
   messages stay in buffer. Data is removed from data_buf by 
   receive()/tmpo_receive(). Messages with unexpected sequences 
   are discarded.
*/

struct TMPO_ReceiveBuffer
{
  unsigned short next_seq;
  struct CQueue *msg_queue, *rbuffer;
};

struct TMPO_Struct
{
  unsigned short other_start_seq, other_seq, me_seq;   // other_seq is the next expected sequence from other
                                                       // me_seq is the next sequence to use in a sent message
  unsigned short last_ack; // last acknowlegement received
  mid_t me, other;
  unsigned char me_fin : 1, other_fin : 1, valid : 1;
  unsigned char state : 4;

  volatile struct TMPO_ReceiveBuffer receive;
  volatile struct TMPO_SendBuffer send;
};

void __init_tmpo_structs(void);
int get_listen_desc(void);
int get_free_tmpo_desc(void);
bool has_tmpo_bytes(int desc);
int create_tmpo_desc(mid_t to, mid_t from);

#define MAX_CONN 8
volatile struct TMPO_Struct tmpo_info[MAX_CONN];

#endif /* TMPO_H */
