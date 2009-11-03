#include <os/rmpo.h>
#include <os/message.h>
#include <oslib.h>
#include <string.h>
#include <os/server.h>
#include <os/adt/cqueue.h>

int create_rmpo_desc(mid_t to, mid_t from);
int rmpo_is_connected(int desc);
int rmpo_get_listen_desc(void);
int find_rmpo_desc(struct Message *msg);
int get_free_rmpo_desc(void);
int _send_rmpo_ack(volatile struct RMPO_Record *record, unsigned ack, void *buf, size_t bytes);
int _send_rmpo_syn(volatile struct RMPO_Record *record, unsigned seq);
int _send_rmpo_syn_ack(volatile struct RMPO_Record *record, unsigned seq, unsigned ack);
int send_rmpo_segment(volatile struct RMPO_Record *record, struct RMPO_Segment *seg);
int get_rmpo_desc(void);
int rmpo_listen(int desc);
int rmpo_open(int desc);
int rmpo_receive(int desc, void *buffer, size_t bytes);

int rmpo_buffer_seg(volatile struct RMPO_Record *record, struct RMPO_Segment *seg);
void rmpo_listen_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg);
void rmpo_syn_sent_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg);
void rmpo_syn_recvd_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg);
void rmpo_opened_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg);
void rmpo_close_wait_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg);

void handle_RMPO(struct Message *msg);
void send_sbuffer(struct RMPO_Record *record); // retransmits unack'ed msgs
int flush_all_acked_msgs(volatile struct RMPO_Record *record, unsigned ack);
int flush_acked_msg(volatile struct RMPO_Record *record, unsigned ack);

#define RMPO_FLG_SET(flag, x)	((x & (flag)) == (flag))

/* XXX: EAK handling is broken. This needs to be fixed before the code
   will start working. */

#define RMPO_NUM_BUF_MSGS	128

int create_rmpo_desc(mid_t to, mid_t from)
{
  int desc = get_free_rmpo_desc();

  rmpo_record[desc].this_mid = from;
  rmpo_record[desc].other_mid = to;

  return desc;
}

// XXX: Unlock the resource

void free_rmpo_desc(int desc)
{
  if( desc < 0 || desc >= RMPO_MAX_CONN )
    return;
}

int rmpo_is_connected(int desc)
{
  if( desc < 0 || desc >= RMPO_MAX_CONN )
    return 0;
  else
    return (rmpo_record[desc].state == RMPO_OPENED);
}

int rmpo_get_listen_desc(void)
{
  int i;

  for(i=0; i < RMPO_MAX_CONN; i++)
  {
    if(rmpo_record[i].state != RMPO_LISTEN)
      continue;
    else
      break;
  }
  return (i >= RMPO_MAX_CONN ? -1 : i);
}

int find_rmpo_desc(struct Message *msg)
{
  int i;
  struct RMPO_Record *record;

  for(i=0, record=rmpo_record; i < RMPO_MAX_CONN; i++,record++)
  {
    if(record->state == RMPO_CLOSED)
      continue;

    if(record->this_mid == msg->recipient && record->other_mid == msg->reply_mid )
        break;
  }
  if( i == RMPO_MAX_CONN )
    return -1;
  else
    return i;
}

int get_free_rmpo_desc(void)
{
  int i;
  struct RMPO_Record *record;

  for(i=0,record=rmpo_record; i < RMPO_MAX_CONN; i++,record++)
  {
    if(record->state == RMPO_CLOSED )
      return i;
  }
  return -1;
}

int rmpo_has_msg(int desc)
{
  if( rmpo_record[desc].recv.app_buffer->state == CQUEUE_EMPTY )
    return 0;
  else
    return 1;
}

int send_rmpo_msg(volatile struct RMPO_Record *record, void *buf, size_t bytes)
{
  struct RMPO_Segment seg;
  unsigned *data;

  if( record == NULL || bytes > sizeof(seg.data) || (buf == NULL && bytes > 0))
    return -1;

  if( bytes > RMPO_MAX_SEGSIZE )
    print("Too many bytes!\n");

  if( bytes > 0 && record->send.num_segs == record->send.max_segs )
    return -2;

  /* XXX: This function may break if too many EACKs need to be sent(more than
     one segment can hold). */

  if( bytes + record->recv.num_segs * sizeof(unsigned) + sizeof seg.hdr > sizeof seg ) // Send two messages for data and EACKs
  {
    if( record->recv.num_segs )
    {
      seg.hdr.flags = RMPOCTRL_ACK | RMPOCTRL_EAK;
      seg.hdr.seq = record->send.next_seq;
      seg.hdr.ack = record->recv.curr_seq;
      seg.hdr.hdr_len = sizeof seg.hdr + sizeof(unsigned) * record->recv.num_segs;
      seg.hdr.data_len = 0;//bytes;
      data = (unsigned *)seg.data;

      for(int i=0; i < record->recv.num_segs; i++, data++)
        *data = record->recv.msg_buf[i].hdr.seq;//record->recv.ack_list[i];

      if( send_rmpo_segment(record, &seg) != 0 )
        return -1;
    }

    if( bytes > 0 )
    {
      seg.hdr.flags = RMPOCTRL_ACK;
      seg.hdr.seq = record->send.next_seq;
      seg.hdr.ack = record->recv.curr_seq;
      seg.hdr.hdr_len = sizeof seg.hdr;
      seg.hdr.data_len = bytes;

      memcpy(data, buf, bytes);
      
      if( send_rmpo_segment(record, &seg) != 0 )
        return -1;
      else
      {
        record->send.msg_buf[record->send.num_segs++] = seg;
        record->send.next_seq++;
        return 0;
      }
    }
    else
      return 0;
  }
  else
  { // send only one message(eack and data combined)
    seg.hdr.flags = RMPOCTRL_ACK;
    seg.hdr.seq = record->send.next_seq;
    seg.hdr.ack = record->recv.curr_seq;
    seg.hdr.hdr_len = sizeof seg.hdr + sizeof(unsigned) * record->recv.num_segs;
    seg.hdr.data_len = bytes;
    data = (unsigned *)seg.data;

    if( record->recv.num_segs )
    {
      seg.hdr.flags |= RMPOCTRL_EAK;

      for(int i=0; i < record->recv.num_segs; i++, data++)
        *data = record->recv.msg_buf[i].hdr.seq;//record->recv.ack_list[i];
    }

    if( bytes > 0 )
      memcpy(data, buf, bytes);

    if( send_rmpo_segment(record, &seg) == 0 )
    {
      if( bytes > 0 )
      {
        record->send.next_seq++;
        record->send.msg_buf[record->send.num_segs++] = seg;
      }

      return 0;
    }
  }
  return -1;
}

int _send_rmpo_ack(volatile struct RMPO_Record *record, unsigned ack, void *buf, size_t bytes)
{
  struct RMPO_Segment seg;

  if( record == NULL )
    return -1;

  seg.hdr.flags = RMPOCTRL_ACK;
  seg.hdr.seq = record->send.next_seq;
  seg.hdr.ack = ack;
  seg.hdr.hdr_len = sizeof seg.hdr;
  seg.hdr.data_len = bytes;

  if( bytes > RMPO_MAX_SEGSIZE )
    print("Too many bytes!\n");

  if( buf != NULL )
    memcpy(seg.data, buf, bytes);

  return send_rmpo_segment(record, &seg);
}

int _send_rmpo_syn(volatile struct RMPO_Record *record, unsigned seq)
{
  struct RMPO_SYN_Segment *syn_seg;
  struct RMPO_Segment seg;

  syn_seg = (struct RMPO_SYN_Segment *)&seg;

  if( record == NULL )
    return -1;

  syn_seg->hdr.flags = RMPOCTRL_SYN;
  syn_seg->hdr.seq = seq;
  syn_seg->seq_enable = record->send.order;
  syn_seg->max_segs = record->recv.max_segs;
  syn_seg->hdr.data_len = 0;
  syn_seg->hdr.hdr_len = sizeof *syn_seg;

  return send_rmpo_segment(record, &seg);
}

int _send_rmpo_syn_ack(volatile struct RMPO_Record *record, unsigned seq, unsigned ack)
{
  struct RMPO_SYN_Segment *syn_seg;
  struct RMPO_Segment seg;

  syn_seg = (struct RMPO_SYN_Segment *)&seg;

  if( record == NULL )
    return -1;

  syn_seg->hdr.flags = RMPOCTRL_SYN | RMPOCTRL_ACK;
  syn_seg->hdr.seq = seq;
  syn_seg->hdr.ack = ack;
  syn_seg->seq_enable = record->send.order;
  syn_seg->max_segs = record->recv.max_segs;
  syn_seg->hdr.data_len = 0;
  syn_seg->hdr.hdr_len = sizeof *syn_seg;

  return send_rmpo_segment(record, &seg);
}

int send_rmpo_segment(volatile struct RMPO_Record *record, struct RMPO_Segment *seg)
{
  if(record == NULL || seg == NULL)
    return -1;

  if( RMPO_FLG_SET(RMPOCTRL_SYN, seg->hdr.flags) )
    print("S");
  if( RMPO_FLG_SET(RMPOCTRL_ACK, seg->hdr.flags) )
    print("A");
  if( RMPO_FLG_SET(RMPOCTRL_EAK, seg->hdr.flags) )
    print("E");
  if( RMPO_FLG_SET(RMPOCTRL_RST, seg->hdr.flags) )
    print("R");
  if( RMPO_FLG_SET(RMPOCTRL_NUL, seg->hdr.flags) )
    print("N");

  print(": "), printHex(seg->hdr.seq), print(",")/*,printHex(seg->hdr.ack),*/, printInt(seg->data[0]), print(" ");
//  printInt(seg->hdr.data_len), print(" ");

/*
//  if( seg->hdr.seq == 0x100 )
//  {
  print("this! ");
  if( RMPO_FLG_SET(RMPOCTRL_SYN, seg->hdr.flags) )
    print("S");
  if( RMPO_FLG_SET(RMPOCTRL_ACK, seg->hdr.flags) )
    print("A");
  if( RMPO_FLG_SET(RMPOCTRL_EAK, seg->hdr.flags) )
    print("E");
  if( RMPO_FLG_SET(RMPOCTRL_RST, seg->hdr.flags) )
    print("R");
  if( RMPO_FLG_SET(RMPOCTRL_NUL, seg->hdr.flags) )
    print("N");

   print(": ");
   printHex(seg->hdr.seq);
   print(",");
   printHex(seg->hdr.ack);
   print(" ");
   printInt(seg->hdr.data_len);
   print("$##$");
//  }

  if( _num_pcts != 1 )
  {
    print("PCT fail");
  }
*/
  return _post_message( &__out_queue, RMPO_PROTOCOL, rmpo_record->other_mid,
                        rmpo_record->this_mid, seg, seg->hdr.data_len + seg->hdr.hdr_len );
}

int get_rmpo_desc(void)
{
  for(int i=0; i < RMPO_MAX_CONN; i++)
  {
    // XXX: May require the use of semaphores
    if( rmpo_record[i].state == RMPO_CLOSED )
      return i;
  }
  
  return -1;
}

int rmpo_listen(int desc)
{
  if( desc < 0 || desc >= RMPO_MAX_CONN )
    return -1;

  if( rmpo_record[desc].state != RMPO_CLOSED )
    return -2; // Connection is already open

  // XXX: May require the use of semaphores

  rmpo_record[desc].send.num_segs = 0;
  rmpo_record[desc].send.first_seq = desc * (unsigned)&desc + 20; 
  rmpo_record[desc].send.next_seq = rmpo_record[desc].send.first_seq + 1;
  rmpo_record[desc].send.old_unack = rmpo_record[desc].send.first_seq;
  rmpo_record[desc].send.order = 1;

  rmpo_record[desc].recv.num_segs = 0;
  rmpo_record[desc].recv.max_segs = 2;//RMPO_MAX_SEG;

  rmpo_record[desc].recv.app_buffer = 
    cqueue_init( sizeof(struct RMPO_Data), RMPO_NUM_BUF_MSGS);

  if( rmpo_record[desc].recv.app_buffer == NULL )
  {
    print("NULL");
    // XXX: Deallocate buffers;
    return -1;
  }

  rmpo_record[desc].state = RMPO_LISTEN;
  return 0;
}

int rmpo_open(int desc)
{
  struct RMPO_Record *record;

  if( desc < 0 || desc >= RMPO_MAX_CONN )
    return -1;

  record = &rmpo_record[desc];

  if( record->state != RMPO_CLOSED )
    return -2; // Connection is already open

  record->send.num_segs = 0;
  record->send.first_seq = 0x100/*(desc+1) * (unsigned)&desc*/; 
  record->send.next_seq = record->send.first_seq + 1;
  record->send.old_unack = record->send.first_seq;
  record->send.order = 1;

  record->recv.num_segs = 0;
  record->recv.max_segs = RMPO_MAX_SEG;

  rmpo_record[desc].recv.app_buffer =
    cqueue_init( sizeof(struct RMPO_Data), RMPO_NUM_BUF_MSGS);

  if( rmpo_record[desc].recv.app_buffer == NULL )
  {
    // XXX: Deallocate buffers;
    return -1;
  }

  record->state = RMPO_SYN_SENT;
  return _send_rmpo_syn(&rmpo_record[desc], record->send.first_seq);
}

int rmpo_receive(int desc, void *buffer, size_t bytes)
{
  volatile struct RMPO_Record *record;
  struct RMPO_Data rmpo_data;
  int status;

  if( desc < 0 || desc >= RMPO_MAX_CONN || rmpo_record[desc].state != RMPO_OPENED )
    return -1;

  record = &rmpo_record[desc];

  status = cqueue_get(record->recv.app_buffer, &rmpo_data, 1);

  if( status < 0 )
    return -1;
  else
  {
    memcpy( buffer, rmpo_data.data, (bytes < rmpo_data.length) ? bytes : rmpo_data.length );
    return rmpo_data.length;
  }
}

int rmpo_send(int desc, void *buffer, size_t bytes)
{
  volatile struct RMPO_Record *record;

  if( desc < 0 || desc >= RMPO_MAX_CONN || rmpo_record[desc].state != RMPO_OPENED
      || (buffer == NULL && bytes > 0) )
  {
    if( desc < 0 || desc >= RMPO_MAX_CONN )
      print("Invalid descriptor\n");
    if( rmpo_record[desc].state != RMPO_OPENED )
      print("Not opened\n");
    return -1;
  }

  record = &rmpo_record[desc];

  if( bytes > RMPO_MAX_SEGSIZE )
  {
    print("Too many bytes!");
    return -1;
  }
  else if( record->send.num_segs >= /*RMPO_MAX_SEG*/ record->send.max_segs )
  {
    send_sbuffer(record); // Try to flush the sbuffer now
//    print("Can't send any more\n");
    return -2;
  }
  // The receiver won't be able to handle any more messages
  else if( record->send.next_seq >= record->send.old_unack + record->send.max_segs )
  {
//    send_sbuffer(record);
    //print("Need to wait for acks\n");
    // Wait for acknowlegements before sending more
    return -3;
  }
/*
  seg = (struct RMPO_Segment *)&record->send.msg_buf[record->send.num_segs];
  memcpy(seg->data, buffer, bytes);
  seg->hdr.flags = RMPOCTRL_ACK;
  seg->hdr.seq = record->send.next_seq;
  seg->hdr.ack = record->recv.curr_seq;
  seg->hdr.hdr_len = sizeof seg->hdr;
  seg->hdr.data_len = bytes;
*/
  /* XXX: Maybe send EAKs here as well? */

//  record->send.num_segs++;

//  if( bytes > 0 )
//    record->send.next_seq++;

  return send_rmpo_msg(record, buffer, bytes);

//  send_sbuffer(record); // Send all messages in the send buffer

//  return 0;
}

void send_sbuffer(struct RMPO_Record *record)
{
  if( record == NULL )
    return;

  for(int i=0; i < record->send.num_segs; i++)
  {
    if( send_rmpo_segment(record, &record->send.msg_buf[i]) != 0 )
      break;
  }
}

/* Buffer an out-of-sequence segment */

int rmpo_buffer_seg(volatile struct RMPO_Record *record, struct RMPO_Segment *seg)
{
  if( record == NULL || seg == NULL )
    return -1;

  /* XXX: EAKs aren't working... */

  if( seg->hdr.seq <= record->recv.curr_seq )
    return -3;

  if(record->recv.num_segs >= record->recv.max_segs)
  {
    //print("Too many segs ");
    return -1;
  }
  else if(seg->hdr.data_len == 0)
    return 2;
  else
  {  // This isn't very efficient
     // This is supposed to keep the segments sorted as they are buffered
    int i=0;

    for(i=0; i < record->recv.num_segs; i++)
    {
      if( record->recv.msg_buf[i].hdr.seq == seg->hdr.seq )
        return 1; // Duplicate
      else if( record->recv.msg_buf[i].hdr.seq > seg->hdr.seq /*&& record->recv.order*/)
        break;
    }

//    if( record->recv.order )
//    {
      for(int j=record->recv.num_segs; j > i; j--)
//      {
        record->recv.msg_buf[j] = record->recv.msg_buf[j-1];
//        record->recv.ack_list[j] = record->recv.ack_list[j-1];
//    }

    record->recv.msg_buf[i] = *seg;
//    record->recv.ack_list[i] = seg->hdr.seq;
    record->recv.num_segs++;
  }
  return 0;
}

void rmpo_listen_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg)
{
  if( seg->hdr.flags == RMPOCTRL_SYN )
  {
    record->recv.first_seq = seg->hdr.seq;
    record->recv.curr_seq = seg->hdr.seq;
    record->recv.max_segs = RMPO_MAX_SEG;

 //   record->send.next_seq = (unsigned)&record * (unsigned)&seg;
    record->send.order = ((struct RMPO_SYN_Segment *)seg)->seq_enable;
    record->send.max_segs = ((struct RMPO_SYN_Segment *)seg)->max_segs;
    record->state = RMPO_SYN_RCVD;

//    print("Recv Max segments: "), printInt(record->send.max_segs), print("!!!");

    _send_rmpo_syn_ack(record, record->send.first_seq, record->recv.curr_seq);
  }
}

void rmpo_syn_sent_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg)
{
  if( RMPO_FLG_SET(RMPOCTRL_SYN, seg->hdr.flags) )
  {
    record->recv.first_seq = seg->hdr.seq;
    record->recv.curr_seq = seg->hdr.seq;
    record->send.order = ((struct RMPO_SYN_Segment *)seg)->seq_enable;
    record->send.max_segs = ((struct RMPO_SYN_Segment *)seg)->max_segs;

//    print("Max segments: "), printInt(record->send.max_segs), print("!!!");

    if( (seg->hdr.flags & RMPOCTRL_ACK) == RMPOCTRL_ACK )
    {
      record->send.old_unack = seg->hdr.ack + 1;
      record->state = RMPO_OPENED;

      _send_rmpo_ack(record, record->recv.curr_seq, NULL, 0);
    }
    else
    {
      record->state = RMPO_SYN_RCVD;
      _send_rmpo_syn_ack(record, record->send.first_seq, seg->hdr.seq);
    }
  }
}

void rmpo_syn_recvd_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg)
{
  if( record->recv.first_seq >= seg->hdr.seq || seg->hdr.seq > record->recv.curr_seq + (2 * record->recv.max_segs) )
  {
    print("Bad sequence number1 "), print(toIntString(seg->hdr.seq)), print(" "), print(toIntString(record->recv.first_seq)), print(" "), print(toIntString(record->recv.curr_seq));
    return;
  }

  if( seg->hdr.flags == RMPOCTRL_ACK)
  {
    if( seg->hdr.ack == record->send.first_seq )
    {
   //   print("ACK received: switching to OPENED ");
      record->state = RMPO_OPENED;
    }
    else
    {
      print("Error out of sequence "), print(toIntString(seg->hdr.ack)), print(" "), print(toIntString(record->send.first_seq));
    }
  }
  else
  {
    print("Something else happened... ");
    print(toHexString(seg->hdr.flags));
    print(" "), print(toIntString(seg->hdr.seq)), print(" "), print(toIntString(seg->hdr.ack));
  }
}

// XXX: May break on wrap-around

/* This removes all queued messages in the send buffer that are waiting for
   their acknowledgements. If the buffer is: (0,1,2,3) and the ack is 2,
   then 0,1,2 are removed. */

int flush_all_acked_msgs(volatile struct RMPO_Record *record, unsigned ack)
{
  int i;

  if( record == NULL )
    return -1;

  if( record->send.num_segs == 0 )
    return 0;

  if( ack > record->send.msg_buf[record->send.num_segs-1].hdr.seq )
  {
    print("Ack is greater than buffered acks!\n");  // Not supposed to happen
    return -1;
  }

  for(i=0; i < record->send.num_segs; i++)
  {
    if( ack < record->send.msg_buf[i].hdr.seq )
    {
      if( i == 0 )
        break;

      for(int j=i; j < record->send.num_segs; j++) // Shift everything left
        record->send.msg_buf[j-i] = record->send.msg_buf[j];

      break;
    }
  }

  record->send.num_segs -= i;
  return i;
}

// XXX: May break on wrap around

/* Remove one queued message in the send buffer according to
   an acknowlegement. */

int flush_acked_msg(volatile struct RMPO_Record *record, unsigned ack)
{
  if( record == NULL )
    return -1;

  for(int i=0; i < record->send.num_segs; i++)
  {
    if( record->send.msg_buf[i].hdr.seq == ack )
    {
      for(int j=i; j < record->send.num_segs - 1; j++)
        record->send.msg_buf[j] = record->send.msg_buf[j+1];

      record->send.num_segs--;
      return 1;
    }
  }
  return 0;
}

int flush_eack_msgs(volatile struct RMPO_Record *record, unsigned seq)
{
  int i;
//  unsigned num_segs;
  struct RMPO_Data rmpo_data;
  struct RMPO_Segment *seg;

  if( record == NULL )
    return -1;

  if( record->recv.num_segs == 0 )
    return 0;

  if( seq >= record->recv.msg_buf[record->recv.num_segs-1].hdr.seq )
  {
    for(int j=0; j < record->recv.num_segs; j++)
    {
      seg = &record->recv.msg_buf[j];
      memcpy(rmpo_data.data, (void *)((unsigned)seg + seg->hdr.hdr_len), seg->hdr.data_len);
      rmpo_data.length = seg->hdr.data_len;

      if( cqueue_put( record->recv.app_buffer, &rmpo_data, 1) != 0 ) // XXX: If something goes wrong(app buffer too full)
      {                                                              // shift the remaining segments left
          print("Ep"), printHex(seg->hdr.seq), print(" ");
        i = j;
        break;
      }
    }

    if( i > 0 )
    {
      for(int j=i; j < record->recv.num_segs; j++) // Shift everything left
        record->recv.msg_buf[j-i] = record->recv.msg_buf[j];
    }

    record->recv.num_segs -= i;
    return i;
  } 

  for(i=0; i < record->recv.num_segs; i++)
  {
    if( seq < record->recv.msg_buf[i].hdr.seq )
    {
      if( i == 0 )
        break;

      for(int j=0; j < i; j++)
      {
        seg = &record->recv.msg_buf[j];
        memcpy(rmpo_data.data, (void *)((unsigned)seg + seg->hdr.hdr_len), seg->hdr.data_len);
        rmpo_data.length = seg->hdr.data_len;

        if( cqueue_put( record->recv.app_buffer, &rmpo_data, 1) != 0 )
        {
          print("Ep"), printHex(seg->hdr.seq), print(" ");
          i = j;
          break;
        }    
      }

      for(int j=i; j < record->recv.num_segs; j++) // Shift everything left
        record->recv.msg_buf[j-i] = record->recv.msg_buf[j];

      break;
    }
  }

  record->recv.num_segs -= i;
  return i;

}

void rmpo_opened_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg)
{
  unsigned num_eacks;
  unsigned *eack;
  struct RMPO_Data rmpo_data;

  if( record->recv.curr_seq >= seg->hdr.seq || 
      seg->hdr.seq > record->recv.curr_seq + record->recv.max_segs )
  {
    send_rmpo_msg(record, NULL, 0);
    return;                         
  }

  if( RMPO_FLG_SET(RMPOCTRL_RST, seg->hdr.flags) )
  {
    record->state = RMPO_CLOSE_WAIT;
    return;
  }

  if( RMPO_FLG_SET(RMPOCTRL_NUL, seg->hdr.flags) )
  {
    record->recv.curr_seq = seg->hdr.seq;
    send_rmpo_msg(record, NULL, 0);
    return;
  }

  if( RMPO_FLG_SET(RMPOCTRL_ACK, seg->hdr.flags) || 
      RMPO_FLG_SET(RMPOCTRL_EAK, seg->hdr.flags ) )
  {
    flush_eack_msgs(record, seg->hdr.seq);

    /* XXX: If the sequence number is greater than an EAK, remove it from the list. */

    if( RMPO_FLG_SET(RMPOCTRL_ACK, seg->hdr.flags) )
    {
      /* Remove the acked messages */

      if( record->send.old_unack <= seg->hdr.ack && seg->hdr.ack < record->send.next_seq ) // XXX: May break on a wrap-around
      {
        flush_all_acked_msgs(record, seg->hdr.ack);
        record->send.old_unack = seg->hdr.ack+1;
      }
//      else
//        print("Bad ack ");
    }

    if( RMPO_FLG_SET(RMPOCTRL_EAK, seg->hdr.flags ) )
    {
      num_eacks = (seg->hdr.hdr_len - sizeof seg->hdr) / sizeof(unsigned);
      eack = (unsigned *)seg->data;

      for( unsigned i=0; i < num_eacks; i++, eack++ )
        flush_acked_msg(record, *eack);
    }

    /* If the message has data in it, buffer it */

    if( seg->hdr.data_len > 0 )
    {
      // if in sequence, or out-of-sequence delivery permitted copy directly to user buffers

      if( seg->hdr.seq == record->recv.curr_seq + 1 || !record->recv.order )
      {
        memcpy(rmpo_data.data, (void *)((unsigned)seg + seg->hdr.hdr_len), seg->hdr.data_len);
        rmpo_data.length = seg->hdr.data_len;

        if( cqueue_put( record->recv.app_buffer, &rmpo_data, 1) == 0 && seg->hdr.seq == record->recv.curr_seq + 1 )
        {
//          print("p"), printHex(seg->hdr.seq), print(":") ,printInt(seg->data[0]), print(" ");
          record->recv.curr_seq = seg->hdr.seq;
        }
      }
      else
        rmpo_buffer_seg(record, seg);

      send_rmpo_msg(record, NULL, 0);
/*
      if( rmpo_buffer_seg(record, seg) == 0 )
      { 
    //XXX: If segment is out of sequence and there is room to buffer it
    //XXX: (wouldn't this be redundant since rmpo_buffer_seg succeeded?)
    //XXX: This assumes that the out of sequence list and receive buffers
    //XXX: are the same size...
        if( seg->hdr.seq == record->recv.curr_seq + 1 ) // If segment is in sequence...
          record->recv.curr_seq = seg->hdr.seq;

        send_rmpo_msg(record, NULL, 0);
      }
      //else
      //  print("No data ");
*/
    }
  }
}

void rmpo_close_wait_handler(volatile struct RMPO_Record *record, struct RMPO_Segment *seg)
{
  // send_rmpo_rst(record);
}

void handle_RMPO(struct Message *msg)
{
  struct RMPO_Segment *seg;
  struct RMPO_Record *record;
  int desc;

  if( msg == NULL || msg->reply_mid == NULL_MID )
    return;

  seg = (struct RMPO_Segment *)msg->data;
  desc = rmpo_get_listen_desc();

  if( desc == -1 )
  {
    desc = find_rmpo_desc(msg);
    record = &rmpo_record[desc];
  }
  else
  {
    record = &rmpo_record[desc];
    record->this_mid = msg->recipient;
    record->other_mid = msg->reply_mid;

    if( msg->reply_mid == NULL_MID || msg->reply_mid == 0 )
    {
      print("Bad reply mid ");
      record->state = RMPO_CLOSED;
      return;
    }

    if( msg->recipient == NULL_MID || msg->recipient == 0 )
    {
      print("Bad recipient mid ");
      record->state = RMPO_CLOSED;
      return;
    }
  }

  if( desc == -1 || record->state == RMPO_CLOSED )
  {
//    if( !seg->rst )
//      _send_rst_msg(msg->reply_mid, msg->recipient);

    print("Bad message ");
    return;
  }

  switch(record->state)
  {
    case RMPO_LISTEN:
      rmpo_listen_handler(record, seg);
      break;
    case RMPO_SYN_SENT:
      rmpo_syn_sent_handler(record, seg);
      break;
    case RMPO_SYN_RCVD:
      rmpo_syn_recvd_handler(record, seg);
      break;
    case RMPO_OPENED:
      rmpo_opened_handler(record, seg);
      break;
    case RMPO_CLOSE_WAIT:
      rmpo_close_wait_handler(record, seg);
      break;
    default:
      print("Got a message in some other mode ");
  }
}
