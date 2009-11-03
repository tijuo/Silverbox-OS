#ifndef RMPO_H
#define RMPO_H

#include <os/message.h>
#include <oslib.h>
#include <os/mutex.h>

// This is really just a modified version of RDPv1 and RDPv2
// RFC 908 and RFC 1151, respectively

enum RMPO_State { RMPO_CLOSED, RMPO_LISTEN, RMPO_SYN_RCVD, RMPO_SYN_SENT,
                   RMPO_OPENED, RMPO_CLOSE_WAIT };

#define RMPO_MAX_SEG		12
#define RMPO_MAX_SEGSIZE	(MAX_MSG_LEN-sizeof(struct RMPO_Header))
#define RMPO_MAX_CONN		16

#define RMPOCTRL_SYN		0x01
#define RMPOCTRL_ACK		0x02
#define RMPOCTRL_EAK		0x04
#define RMPOCTRL_RST		0x08
#define RMPOCTRL_NUL		0x10

struct RMPO_SYN_Segment
{
  struct RMPO_Header hdr;
  unsigned short max_segs: 15;   // Maximum number of unack'ed segs that can be sent
  unsigned short seq_enable : 1; // Enables the ordering of sequences
} __PACKED__;

struct RMPO_Segment
{
  struct RMPO_Header hdr;
  unsigned char data[RMPO_MAX_SEGSIZE];
} __PACKED__;

struct RMPO_Data
{
  unsigned char data[RMPO_MAX_SEGSIZE];
  size_t length;
};

struct RMPO_Record
{
  enum RMPO_State state;
  mutex_t lock;

  struct
  {
    unsigned next_seq; // next sequence to be sent
    unsigned old_unack; // oldest unack'ed sequence(may be at position 0 in msg_buf)
    unsigned short max_segs;  // maximum number of unack'ed segments that can be buffered
    unsigned first_seq; // initial send SYN seq number
    struct RMPO_Segment msg_buf[RMPO_MAX_SEG];
    unsigned short num_segs;
    unsigned char order; // Sent segments should be sequenced in order(other side's responsibility)
  } send;

  struct
  {
    unsigned curr_seq; // last correctly received seq
    unsigned short max_segs; // maximum number of segments that can be buffered
//    unsigned short max_acks; // maximum number of acks for out-of-seq msgs
    unsigned first_seq; // initial receive SYN seq number
    // XXX: Separate this
    struct RMPO_Segment msg_buf[RMPO_MAX_SEG]; // buffer for out-of-seq segments
    
    unsigned ack_list[RMPO_MAX_SEG]; // A list of acked sequences
//    unsigned num_acks;
    unsigned short num_segs; // number of segments received
    unsigned char order; // Received segments should be sequenced in order(our responsibility)
//    struct RMPO_Data *app_buffer;
    struct CQueue *app_buffer;
  } recv;

  mid_t this_mid, other_mid;
  // time_t closewait_timer;
};

volatile struct RMPO_Record rmpo_record[RMPO_MAX_CONN];

#endif /* RMPO_H */
