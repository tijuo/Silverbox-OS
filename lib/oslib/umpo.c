#include <oslib.h>
#include <os/message.h>
#include <os/umpo.h>

void handleUMPO(struct Message *msg)
{

}

int _sendto(mid_t port, mid_t reply, unsigned short subj, void *data, 
size_t len, int dont_frag)
{
  struct UMPO_Header *header;
  struct Message msg;

// XXX: fragmented packets aren't supported yet

  if(len > MAX_MSG_LEN - sizeof(struct UMPO_Header))                                                
    return -1;

  msg.recipient = port;
  msg.reply_mid = reply;
  msg.length = len;
  msg.protocol = UMPO_PROTOCOL;

  header = (struct UMPO_Header *)msg.data;

  header->subject = subj;
  header->flags = 0;
  header->frag_id = 0;
  header->resd = 0;

  memcpy((header+1), data, len);

  return __post_message(&__out_queue, &msg);
}
