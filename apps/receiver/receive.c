#include <os/syscalls.h>

extern void print(const char *);
extern void printInt(int);
extern void printHex(int);

int main(void)
{
  msg_t msg =
  {
    .sender = ANY_SENDER,
  };

  while(1)
  {
    if(sys_receive(&msg, 1) == ESYS_OK)
    {
      print("receiver: Received a message with subject "), printInt(msg.subject), print(" from "), printInt(msg.sender), print("\n");

      if(msg.sender == 1026)
      {
        for(int i=0; i < 5; i++)
        {
          print("0x");
          printHex(msg.data.i32[i]);
          print(" ");
        }
        print("\n");
        break;
      }
      else
        msg.sender = ANY_SENDER;
    }
    else
    {
      print("Error. Unable to receive message");
      return 1;
    }
  }

  return 0;
}
