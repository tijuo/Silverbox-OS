#include <os/syscalls.h>
#include <os/msg/message.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  msg_t msg = {
    .subject = 69,
    .recipient = 1027,
    .data = {
      .u32 = { 0xCAFEBABE, 0xDEADBEEF, 0x1BADD00D, 0xC0FFEE, 0xF00DFACE }
    }
  };
/*
  msg.subject = 69;
  msg.recipient = 1027;
  msg.data.u32 = { 0xCAFEBABE, 0xDEADBEEF, 0x1BADD00D, 0xC0FFEE, 0xF00DFACE };
*/
  sys_wait(1000);

  if(sys_send(&msg, 1) == ESYS_OK)
    return EXIT_SUCCESS;
  else
    return EXIT_FAILURE;
}
