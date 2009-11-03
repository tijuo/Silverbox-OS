#include <os/tmpo.h>
#include <os/message.h>
#include <limits.h>
#include <oslib.h>
#include <string.h>

#define RBUFFER_LEN	65536
#define RMQUEUE_LEN	256
#define SBUFFER_LEN	65536
#define SMQUEUE_LEN	64

int buffer_sent_data( struct TMPO_Struct *t_struct, void *data, size_t bytes );
int buffer_recvd_msg( struct TMPO_Struct *t_struct, struct Message *msg );
int between_incl(unsigned val, unsigned start, unsigned end, unsigned limit);

void __init_tmpo_structs(void)
{
  memset(tmpo_info, 0, sizeof tmpo_info);
}

int send_tmpo_msg(struct TMPO_Struct *tmpo_struct, int flags,
                  void *data, size_t bytes)
{
  struct Message msg;
  struct TMPO_Header *header = (struct TMPO_Header *)msg.data;
  mid_t to, from;

  if(tmpo_struct == NULL || bytes > MAX_MSG_LEN - sizeof(struct TMPO_Header))
    return -1;

  if( data != NULL && bytes > 0 )
    memcpy(header+1, data, bytes);
  else
    bytes = 0;

  to = tmpo_struct->other;
  from = tmpo_struct->me;
  header->seq = tmpo_struct->me_seq;
  header->ack = tmpo_struct->other_seq;

  msg.protocol = TMPO_PROTOCOL;
  msg.reply_mid = from;
  msg.recipient = to;
  msg.length = bytes + sizeof *header;

//  header->window = 0; XXX: Need to implement window
  header->window = 1;
  header->flags = flags;

  return __post_message( &__out_queue, &msg );
}

int _send_syn_msg(mid_t to, mid_t from)
{
  struct Message msg;
  struct TMPO_Header *header = (struct TMPO_Header *)msg.data;

   //print("Send SYN ");

  msg.protocol = TMPO_PROTOCOL;
  msg.reply_mid = from;
  msg.recipient = to;
  msg.length = sizeof *header;

//  header->window = 0; XXX: Need to implement window
  header->window = 1;
  header->flags = TMPO_SYN;

  return __post_message( &__out_queue, &msg );
}

int _send_rst_msg(mid_t to, mid_t from)
{
  struct Message msg;
  struct TMPO_Header *header = (struct TMPO_Header *)msg.data;

   //print("Send RST ");

  msg.protocol = TMPO_PROTOCOL;
  msg.reply_mid = from;
  msg.recipient = to;
  msg.length = sizeof *header;

  header->window = 0;
  header->flags = TMPO_RST;

  return __post_message( &__out_queue, &msg );
}

int send_syn_msg(struct TMPO_Struct *tmpo_struct)
{
   //print("send SYN ");
  return send_tmpo_msg(tmpo_struct, TMPO_SYN, NULL, 0);
}

int send_syn_ack_msg(struct TMPO_Struct *tmpo_struct)
{
  //print("send SYN/ACK ");
  return send_tmpo_msg(tmpo_struct, TMPO_SYN | TMPO_ACK, NULL, 0);
}

int send_rst_msg(struct TMPO_Struct *tmpo_struct)
{
   //print("Send RST ");
  return send_tmpo_msg(tmpo_struct, TMPO_RST, NULL, 0);
}

int send_ack_msg(struct TMPO_Struct *tmpo_struct, void *buf, size_t bytes)
{
   //print("Send ACK ");
  return send_tmpo_msg(tmpo_struct, TMPO_ACK, buf, bytes);
}

int get_listen_desc(void)
{
  int i;

  for(i=0; i < MAX_CONN; i++)
  {
    if(tmpo_info[i].state != LISTEN)
      continue;
    else
      break;
  }
  return (i >= MAX_CONN ? -1 : i);
}

int find_tmpo_desc(struct Message *msg)
{
  int i;

  for(i=0; i < MAX_CONN; i++)
  {
    if(tmpo_info[i].state == CLOSED)
      continue;

    if(tmpo_info[i].me == msg->recipient && tmpo_info[i].other == msg->reply_mid )
        break;
  }
  if( i == MAX_CONN )
    return -1;
  else
    return i;
}

int get_free_tmpo_desc(void)
{
  int i;

  for(i=0; i < MAX_CONN; i++)
  {
    if(tmpo_info[i].state == CLOSED)
      break;
  }

  if( i == MAX_CONN )
    return -1;
  else
    return i;
}

/*
  0. While there is room in the data buffer and messages in 
     the message queue, add the message data to the data buffer.
  1. Add new message into msg queue. If no room, drop.
  2. Dequeue message in sequence(if possible) and add data into data buffer.
     If not enough room in the data buffer, stop loop.
  3. Loop to 2
  4. Update next_sequence.
*/

int between_incl(unsigned val, unsigned start, unsigned end, unsigned limit)
{
  limit++;
  
  if( limit != 0 )
  {
    val %= limit;
    start %= limit;
    end %= limit;

//    val = val < 0 ? val + limit : val;
//    start = start < 0 ? start + limit : start;
//    end = end < 0 ? end + limit : end;
  }

  if( end < start )
    return (val > end && val < start ? 0 : 1);
  else
    return (val > end || val < start ? 0 : 1);
}

int buffer_recvd_msg( struct TMPO_Struct *t_struct, struct Message *msg )
{
  struct TMPO_Header *header;
  unsigned char *msg_data;
  struct TMPO_BufMessage tmpo_buf_msg;
  size_t pos;

  if( t_struct == NULL || msg == NULL )
    return -1;

  header = (struct TMPO_Header *)(msg->data);
  msg_data = (unsigned char *)(header+1);

  // while there is sufficent room to place buffered message data in the data buffer
  // and the buffered message is valid

  while(1)
  {
    cqueue_peek( t_struct->receive.msg_queue, &tmpo_buf_msg, 
                 t_struct->receive.msg_queue->head, 1 );

    if(!tmpo_buf_msg.valid || CQUEUE_NUM_FREE( t_struct->receive.rbuffer ) < tmpo_buf_msg.len)
      break;

    tmpo_buf_msg.valid = 0;

    cqueue_poke( t_struct->receive.msg_queue, &tmpo_buf_msg,
                 t_struct->receive.msg_queue->head, 1);
    cqueue_get( t_struct->receive.msg_queue, &tmpo_buf_msg, 1 ); // This removes the message
    cqueue_put( t_struct->receive.rbuffer, tmpo_buf_msg.buffer, tmpo_buf_msg.len );
    t_struct->receive.next_seq++;
  }

  // If the new message can't fit, don't do anything else

  if( !between_incl(header->seq, t_struct->receive.next_seq, 
                    t_struct->receive.next_seq + 
                    t_struct->receive.msg_queue->n_elems,
                    USHRT_MAX) )
  {
/*
    print("Message can't fit! ");
    print(toIntString(header->seq)), print(" "), print(toIntString(t_struct->receive.next_seq)), print(" "), 
print(toIntString(t_struct->receive.msg_queue->n_elems));*/
    return -1;
  }
  else
  {
    // Put the new message on the queue
//    print("Putting message on queue ");
//    print(toIntString(header->seq));

//    cqueue_peek( t_struct->receive.msg_queue, &tmpo_buf_msg,
//                 t_struct->receive.msg_queue->head, 1 );

//    print(" "), print(toIntString(t_struct->receive.next_seq));

    if( header->seq < t_struct->receive.next_seq )
      pos = t_struct->receive.next_seq + (1+USHRT_MAX - t_struct->receive.next_seq);
    else
      pos = header->seq - t_struct->receive.next_seq;

    // XXX: What if a duplicate message has arrived(message was already acked?)

    tmpo_buf_msg.valid = 1;
    tmpo_buf_msg.len = msg->length - sizeof(struct TMPO_Header);
    tmpo_buf_msg.seq = header->seq;
    memcpy(tmpo_buf_msg.buffer, msg_data, msg->length - sizeof(struct TMPO_Header));

    cqueue_poke( t_struct->receive.msg_queue, &tmpo_buf_msg,
                 (t_struct->receive.msg_queue->head + pos) %
                 t_struct->receive.msg_queue->n_elems, 1 );
  }

  while(1)
  {
    cqueue_peek( t_struct->receive.msg_queue, &tmpo_buf_msg,
                 t_struct->receive.msg_queue->head, 1 );

    if(!tmpo_buf_msg.valid || CQUEUE_NUM_FREE( t_struct->receive.rbuffer ) < tmpo_buf_msg.len)
      return 0;

    tmpo_buf_msg.valid = 0;

    cqueue_poke( t_struct->receive.msg_queue, &tmpo_buf_msg,
                 t_struct->receive.msg_queue->head, 1);
    cqueue_get( t_struct->receive.msg_queue, &tmpo_buf_msg, 1 );
    cqueue_put( t_struct->receive.rbuffer, tmpo_buf_msg.buffer, tmpo_buf_msg.len );

    if( t_struct->receive.next_seq != tmpo_buf_msg.seq )
      print("Seq numbers don't match\n"); // Error. Not supposed to happen!

    t_struct->receive.next_seq++;
  }
}

int buffer_sent_data( struct TMPO_Struct *t_struct, void *data, size_t bytes )
{
  struct TMPO_BufMessage tmpo_buf_msg;
  size_t data_len;

  if( t_struct == NULL || data == NULL )
    return -1;

  // if possible, flush the data buffer by placing the contents
  // into the message queue

  while(!CQUEUE_IS_FULL(t_struct->send.msg_queue) && !CQUEUE_IS_EMPTY(t_struct->send.sbuffer))
  {
    data_len = (CQUEUE_NUM_FILL(t_struct->send.sbuffer) > 
                 sizeof tmpo_buf_msg.buffer ?
                 sizeof tmpo_buf_msg.buffer :
               CQUEUE_NUM_FILL(t_struct->send.sbuffer));

    cqueue_get( t_struct->send.sbuffer, tmpo_buf_msg.buffer, data_len );
    tmpo_buf_msg.len = data_len;
    tmpo_buf_msg.seq = t_struct->send.next_seq++;
    tmpo_buf_msg.valid = 1; // Seems redundant
    cqueue_put( t_struct->send.msg_queue, &tmpo_buf_msg, 1 );
  }

  t_struct->me_seq = t_struct->send.next_seq;

  if( bytes > CQUEUE_NUM_FREE(t_struct->send.sbuffer) )
  {
    print("Too many bytes!\n");
    return -1;
  }
  else
  {
    cqueue_put( t_struct->send.sbuffer, data, bytes );

    while(!CQUEUE_IS_FULL(t_struct->send.msg_queue) && !CQUEUE_IS_EMPTY(t_struct->send.sbuffer))
    {
      data_len = (CQUEUE_NUM_FILL(t_struct->send.sbuffer) >
                 sizeof tmpo_buf_msg.buffer ?
                 sizeof tmpo_buf_msg.buffer :
                 CQUEUE_NUM_FILL(t_struct->send.sbuffer));

      cqueue_get( t_struct->send.sbuffer, tmpo_buf_msg.buffer, data_len );
      tmpo_buf_msg.len = data_len;
      tmpo_buf_msg.seq = t_struct->send.next_seq++;
      tmpo_buf_msg.valid = 1; // Seems redundant
      cqueue_put( t_struct->send.msg_queue, &tmpo_buf_msg, 1 );
    }
  }
  t_struct->me_seq = t_struct->send.next_seq;
  return 0;
}

int send_buffered_msgs(struct TMPO_Struct *t_struct)
{
  struct Message msg;
  struct TMPO_BufMessage buf_msg;
  struct TMPO_Header *header = (struct TMPO_Header *)msg.data;
  size_t num_msgs;

  if( t_struct->state != ESTABLISHED )
  {
    print("Sending while not in ESTABLISHED\n");
    return -1; // XXX: Not supposed to happen!
  }
  num_msgs = CQUEUE_NUM_FILL(t_struct->send.msg_queue);

  print(toIntString(num_msgs));

  for(int i=0; num_msgs--; i++)
  {
    if( __MQUEUE_FULL(&__out_queue) )
    {
      if( __send() < 0 )
        return 1;
    }

    cqueue_peek(t_struct->send.msg_queue, &buf_msg, 
      t_struct->send.msg_queue->head + i, 1);

    msg.recipient = t_struct->other;
    msg.reply_mid = t_struct->me;
    msg.length = sizeof *header + buf_msg.len;
    msg.protocol = TMPO_PROTOCOL;
    header->seq = buf_msg.seq;
    header->ack = t_struct->other_seq;
    header->window = 1; // not implemented yet;

    if( t_struct->state == ESTABLISHED )
      header->flags = TMPO_ACK;
    else
      print("error: Not in ESTABLISHED\n");
    memcpy(header+1, buf_msg.buffer, buf_msg.len);
    __post_message( &__out_queue, &msg );
  }
  return 0;
}

int receive_buffered_data(struct TMPO_Struct *t_struct, void *buf, size_t bytes)
{
  size_t data_len = (bytes > CQUEUE_NUM_FILL(t_struct->receive.rbuffer) ?
                    CQUEUE_NUM_FILL(t_struct->receive.rbuffer) : bytes);

  return cqueue_get( t_struct->receive.rbuffer, buf, data_len );
}

int create_tmpo_desc(mid_t to, mid_t from)
{
  int desc = get_free_tmpo_desc();

  tmpo_info[desc].me = from;
  tmpo_info[desc].other = to;

  return desc;
}

int is_connected(int desc)
{
  if( desc < 0 || desc >= MAX_CONN )
    return 0;
  else
    return (tmpo_info[desc].state == ESTABLISHED);
}

int connect(int desc)
{
  if( desc < 0 || desc >= MAX_CONN)
    return -1;

  tmpo_info[desc].me_seq = (desc * (unsigned)&desc) & 0xFFFF;

  if( tmpo_info[desc].me_seq == 0xFFFF )
    tmpo_info[desc].me_seq = 0;

  send_syn_msg(&tmpo_info[desc]);
  tmpo_info[desc].state = SYN_SENT;
  tmpo_info[desc].me_seq++;

  print("SYN: "), print(toIntString(tmpo_info[desc].me_seq-1)), print(" ");

//  __send();
  return 0;
}

int is_waiting(int desc) // Is it accepting connections?
{
  if( desc < 0 || desc >= MAX_CONN )
    return -1;

  if( tmpo_info[desc].state == LISTEN )
    return 1;
  else
    return 0;
}

int accept(int desc)
{
  if( desc < 0 || desc >= MAX_CONN )
    return -1;

  tmpo_info[desc].state = LISTEN;
  return 0;
}

int get_recvd_bytes(int desc)
{
  if( desc < 0 || desc >= MAX_CONN )
    return -1;

  return CQUEUE_NUM_FILL(tmpo_info[desc].receive.rbuffer);
}

int flush_sbuffer(int desc)
{
  struct TMPO_Struct *t_struct;

  if( desc < 0 || desc >= MAX_CONN )
    return -1;

  t_struct = &tmpo_info[desc];

  return _flush_sbuffer(t_struct);
}

int _flush_sbuffer(struct TMPO_Struct *t_struct) // Removes acked messages
{
  struct TMPO_BufMessage tmpo_buf_msg;

  while(!CQUEUE_IS_EMPTY(t_struct->send.msg_queue)) // Remove acknowledged messages
  {
    cqueue_peek( t_struct->send.msg_queue, &tmpo_buf_msg,
                 t_struct->send.msg_queue->head, 1 );

    if(!tmpo_buf_msg.valid/* || tmpo_buf_msg.seq > t_struct->last_ack*/) // XXX: May cause problems on wraparound
      break;

    tmpo_buf_msg.valid = 0;
    cqueue_poke( t_struct->send.msg_queue, &tmpo_buf_msg,
                 t_struct->send.msg_queue->head, 1 );
    cqueue_get( t_struct->send.msg_queue, NULL, 1 );
  }
  return send_buffered_msgs(t_struct);
}

int send(int desc, void *buf, size_t bytes)
{
  if( desc < 0 || desc >= MAX_CONN || buf == NULL)
    return -1;
  else if( tmpo_info[desc].state != ESTABLISHED )
  {
    print("Sending while not in ESTABLISHED\n");
    return -1;
  }

  return buffer_sent_data(&tmpo_info[desc], buf, bytes);
  // If the buffer isn't flushed, nothing will be sent!
}

int receive(int desc, void *buf, size_t bytes)
{
  if (desc < 0 || desc >= MAX_CONN || buf == NULL )
    return -1;
  else if( tmpo_info[desc].state != ESTABLISHED )
  {
    print("Receiving while not in ESTABLISHED\n");
    return -1;
  }

  return receive_buffered_data(&tmpo_info[desc], buf, bytes);
}

void listen_handle( struct TMPO_Struct *tmpo_struct, struct Message *msg )
{
  struct TMPO_Header *header;

  if( tmpo_struct == NULL || msg == NULL )
    return;

  header = (struct TMPO_Header *)msg->data;

  if( header->rst )
    return;
  else if( header->flags != TMPO_SYN )
    _send_rst_msg(msg->reply_mid, msg->recipient);
  else
  {
    tmpo_struct->other_seq = header->seq+1;
    tmpo_struct->me_seq = 10;//(((unsigned long)tmpo_struct * (unsigned long)&msg) & 0xFFFF;
    tmpo_struct->me = msg->recipient;
    tmpo_struct->other = msg->reply_mid;
    tmpo_struct->state = SYN_RECEIVED;
 
    tmpo_struct->receive.rbuffer = cqueue_init( sizeof(unsigned char), RBUFFER_LEN);
    tmpo_struct->receive.msg_queue = cqueue_init( sizeof(struct TMPO_BufMessage), RMQUEUE_LEN);
    tmpo_struct->receive.next_seq = tmpo_struct->other_seq;
    memset(tmpo_struct->receive.msg_queue->queue, 0, tmpo_struct->receive.msg_queue->n_elems * tmpo_struct->receive.msg_queue->elem_size);
  
    tmpo_struct->send.sbuffer = cqueue_init( sizeof(unsigned char), SBUFFER_LEN);
    tmpo_struct->send.msg_queue = cqueue_init( sizeof(struct TMPO_BufMessage), SMQUEUE_LEN);
    tmpo_struct->send.next_seq = tmpo_struct->me_seq+1;

print("SYN/ACK: "), print(toIntString(tmpo_struct->me_seq)), print(" "), print(toIntString(tmpo_struct->other_seq)), print(" ");

    //print("SYN_RECEIVED state ");
    send_syn_ack_msg(tmpo_struct);
    tmpo_struct->me_seq++;
  }
}

void syn_sent_handle( struct TMPO_Struct *tmpo_struct, struct Message *msg )
{
  struct TMPO_Header *header;

  if( tmpo_struct == NULL || msg == NULL )
    return;

  header = (struct TMPO_Header *)msg->data;

  if( header->rst )
  {
    print("RST received\n");
    return; // XXX: Do clean-up here
  }
  else if( header->flags == TMPO_SYN )
  {
    print("TMPO_SYN sent(not supposed to happen!)\n");
    return; // Either send a ACK and switch to SYN_RECEIVED or send and RST
  }
  else if(header->syn && header->ack)
  {
    if( tmpo_struct->me_seq == header->ack /*&& header->ack == tmpo_struct->last_ack + 1*/ )
    {
//      tmpo_struct->last_ack = header->ack;
      tmpo_struct->state = ESTABLISHED;
      tmpo_struct->other_seq = header->seq+1;
      tmpo_struct->send.sbuffer = cqueue_init( sizeof(unsigned char), SBUFFER_LEN);
      tmpo_struct->send.msg_queue = cqueue_init( sizeof(struct TMPO_BufMessage), SMQUEUE_LEN);
      tmpo_struct->send.next_seq = tmpo_struct->me_seq;
      tmpo_struct->receive.rbuffer = cqueue_init( sizeof(unsigned char), RBUFFER_LEN);
      tmpo_struct->receive.msg_queue = cqueue_init( sizeof(struct TMPO_BufMessage), RMQUEUE_LEN);
      tmpo_struct->receive.next_seq = tmpo_struct->other_seq; 
      memset(tmpo_struct->receive.msg_queue->queue, 0, tmpo_struct->receive.msg_queue->n_elems * tmpo_struct->receive.msg_queue->elem_size);

print("ACK: "), print(toIntString(tmpo_struct->me_seq)), print(" "), print(toIntString(tmpo_struct->other_seq)), print(" ");

      send_ack_msg(tmpo_struct, NULL, 0);
      
      //print("ESTABLISHED state ");
      return;
    }
    else
    {
      print("Bad ack number: ");
      print(toIntString(header->ack));
      print(" ");
      print(toIntString(tmpo_struct->last_ack));
      print(" ");
      print(toIntString(header->seq));
      print("\n");
    }
  }
  else
  {
    print("Didn't receive SYN/ACK!\n");
  }
//  send_ack_msg(tmpo_struct);
}

void syn_received_handle( struct TMPO_Struct *tmpo_struct, struct Message *msg )
{
  struct TMPO_Header *header;

  if( tmpo_struct == NULL || msg == NULL )
    return;

  header = (struct TMPO_Header *)msg->data;

  if( header->rst )
  {
    print("RST received\n");
    return; // XXX: Do clean-up here
  }
  else if( header->syn )
  {
    print("TMPO_SYN sent(not supposed to happen!)\n");
    return; // Either send a ACK and switch to SYN_RECEIVED or send and RST
  }
  else if(header->ack)
  {
    if( tmpo_struct->me_seq == header->ack /*&& header->ack == tmpo_struct->last_ack + 1*/ ) // XXX: This may break on wraparound
    {
//      tmpo_struct->last_ack++;
      tmpo_struct->state = ESTABLISHED;

      //print("ESTABLISHED state ");
    }
    else
    {
      print("Bad ack number: ");
      print(toIntString(header->ack));
      print(" ");
      print(toIntString(tmpo_struct->last_ack));
      print(" ");
      print(toIntString(header->seq));
      print("\n");
      return;
    }

// XXX: Hmm...

    buffer_recvd_msg( tmpo_struct, msg );
    tmpo_struct->other_seq = tmpo_struct->receive.next_seq;

    send_ack_msg(tmpo_struct, NULL, 0);
  }
  else
  {
     print("Received strange message\n");
     return;
  }
}

void established_handle(struct TMPO_Struct *t_struct, struct Message *msg)
{
  struct TMPO_Header *header;
//  struct TMPO_BufMessage tmpo_buf_msg;

  if( t_struct == NULL || msg == NULL )
    return;

  header = (struct TMPO_Header *)msg->data;

  if( t_struct->send.sbuffer == NULL || t_struct->send.msg_queue == NULL ||
      t_struct->receive.rbuffer == NULL || t_struct->receive.msg_queue == NULL )
  {
    print("Null structure!\n");
    return;
  }

  if( header->rst )
  {
    print("RST received\n");
    return; // XXX: Do clean-up here
  }
  else if( header->syn )
  {
    print("Received an SYN during ESTABLISHED\n");//_send_rst_msg();
    return;
  }
  else if( header->ack )
  {
    if( /*header->ack > t_struct->last_ack && */header->ack <= t_struct->me_seq ) // will break if the ack wraps around
    {
     // t_struct->last_ack = header->ack; // XXX: What if the ack isn't valid?

      print("OK: ");
      print(toIntString(header->ack));
      print(" ");
      print(toIntString(t_struct->last_ack));
      print(" ");
      print(toIntString(t_struct->other_seq));
      print(" ");
      print(toIntString(t_struct->me_seq));
      print(" ");

      buffer_recvd_msg( t_struct, msg );
      t_struct->other_seq = t_struct->receive.next_seq;
      _flush_sbuffer( t_struct );
      return;
    }
    else
    {
      print("Incorrect ack: ");
      print(toIntString(header->ack));
      print(" ");
      print(toIntString(t_struct->last_ack));
      print(" ");
      print(toIntString(t_struct->other_seq));
      print(" ");
      print(toIntString(t_struct->me_seq));
      print(" ");
      //while(1);
      return;
    }
//    else
//      send_ack_msg(t_struct, NULL, 0);
  }
  else if( !header->ack )
  {
    print("Received DNS\n");
    return;
  }
  else
  {
    print("Strange message received\n");
    return;
  }

 // buffer_recvd_msg( t_struct, msg );
 // t_struct->other_seq = t_struct->receive.next_seq;
//  send_ack_msg(t_struct, NULL, 0);
}

void handleTMPO_New( struct Message *msg )
{
  struct TMPO_Header *header;
  int desc;

  if( msg == NULL || msg->reply_mid == NULL_MID )
    return;

  header = (struct TMPO_Header *)msg->data;

  desc = get_listen_desc();

  if( desc == -1 )
    desc = find_tmpo_desc(msg);

  if( desc == -1 || tmpo_info[desc].state == CLOSED )
  {
    if( !header->rst )
      _send_rst_msg(msg->reply_mid, msg->recipient);

    print("Bad message: sending RST ");
    return;
  }

  switch(tmpo_info[desc].state)
  {
    case SYN_SENT:
//      print("In SYN_SENT: ");
      syn_sent_handle(&tmpo_info[desc], msg);
      break;
    case SYN_RECEIVED:
  //    print("In SYN_RECEIVED: ");
      syn_received_handle(&tmpo_info[desc], msg);
      break;
    case ESTABLISHED:
     // print("In ESTABLISHED: ");
      established_handle(&tmpo_info[desc], msg);
      break;
    case LISTEN:
     // print("\nIn LISTEN: ");
      listen_handle(&tmpo_info[desc], msg);
      break;
    case FIN_WAIT1:
      break;
    case FIN_WAIT2:
      break;
    case CLOSING:
      break;
    case LAST_ACK:
      break;
    case CLOSE_WAIT:
      break;
    case TIME_WAIT:
      break;
    default:
      break;
  }
}
