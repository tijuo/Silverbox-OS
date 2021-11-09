#include <os/services.h>
#include <os/syscalls.h>
#include <os/msg/message.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
  if(registerName("sender") != 0)
    fprintf(stderr, "Unable to register name: sender\n");
  else
    fprintf(stderr, "sender registered successfully.\n");

  tid_t receiverTid;

  for(int i=0; i < 10; i++)
  {
    receiverTid = lookupName("receiver");

    if(receiverTid != NULL_TID)
      break;

    sys_sleep(500);
  }

  if(receiverTid == NULL_TID)
  {
    fprintf(stderr, "Unable to locate receiver.\n");
    return EXIT_FAILURE;
  }

  int ibuf[5] = { 0xCAFEBABE, 0xDEADBEEF, 0x1BADD00D, 0xC0FFEE, 0xF00DFACE };
  size_t bufLen = 16384;
  unsigned char *buf = malloc(bufLen);

  if(!buf)
  {
    fprintf(stderr, "Unable to allocate memory for buffer\n");
    return EXIT_FAILURE;
  }

  msg_t msg = {
    .subject = 123456789,
    .recipient = receiverTid,
    .buffer = ibuf,
    .bufferLen = 20,
    .flags = 0
  };
/*
  msg.subject = 69;
  msg.recipient = 1027;
  msg.data.u32 = { 0xCAFEBABE, 0xDEADBEEF, 0x1BADD00D, 0xC0FFEE, 0xF00DFACE };
*/

  fprintf(stderr, "Sending message to receiver (%d)...\n", receiverTid);

  if(sys_send(&msg) == ESYS_OK)
  {
    msg_t msg2 = {
      .subject = 100,
      .recipient = receiverTid,
      .buffer = buf,
      .bufferLen = bufLen,
      .flags = 0
    };

    for(size_t i=0; i < bufLen; i++)
      buf[i] = (unsigned char)(i % 256);

    fprintf(stderr, "Sending second message to 1027\n");

    if(sys_send(&msg2) == ESYS_OK)
    {
      fprintf(stderr, "Messages sent successfully\n");
      free(buf);
      return EXIT_SUCCESS;
    }
  }

  fprintf(stderr, "Failed to send message\n");
  free(buf);

  return EXIT_FAILURE;
}
