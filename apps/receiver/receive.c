#include <os/services.h>
#include <os/syscalls.h>
#include <oslib.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
  if(registerName("receiver") != 0)
    fprintf(stderr, "Unable to register name: receiver\n");
  else
    fprintf(stderr, "Receiver registered successfully.\n");

  tid_t senderTid;

  for(int i=0; i < 10; i++)
  {
    senderTid = lookupName("sender");

    if(senderTid != NULL_TID)
      break;

    sys_sleep(500);
  }

  if(senderTid == NULL_TID)
  {
    fprintf(stderr, "Unable to locate sender.\n");
    return EXIT_FAILURE;
  }

  size_t bufLen = 4096;
  unsigned char *buf = malloc(bufLen);

  if(!buf)
  {
    fprintf(stderr, "Unable to allocate memory for buffer\n");
    return EXIT_FAILURE;
  }

  while(1)
  {
    msg_t msg =
    {
      .sender = ANY_SENDER,
      .buffer = buf,
      .bufferLen = 4096,
      .flags = 0
    };

    if(sys_receive(&msg) == ESYS_OK)
    {
      fprintf(stderr, "Received a message (subj: %d) from tid: %d of length %d bytes\n", msg.subject, msg.sender, msg.bytesTransferred);

      if(msg.sender == senderTid && msg.bytesTransferred == 20)
      {
        int *ptr = (int *)buf;

        fprintf(stderr, "Message data:");

        for(size_t i=0; i < 5; i++)
          fprintf(stderr, " 0x%x", ptr[i]);

        fprintf(stderr, "\n");
      }
      else if(msg.sender == senderTid && msg.bytesTransferred == 4096)
      {
        for(size_t i=0; i < 4096; i++)
        {
          if(buf[i] != (unsigned char)(i % 256))
          {
            fprintf(stderr, "Invalid data sent.\n");
            free(buf);
            break;
          }
        }
        fprintf(stderr, "Data received successfully.\n");
      }

    }
    else
      sys_sleep(300);

/*
    else
    {
      fprintf(stderr, "Error. Unable to receive message\n");
      return 1;
    }
*/
  }

  free(buf);
  return 0;
}
