#include <os/syscalls.h>

int main(void)
{
  msg_t msg =
  {
    .recipient = 1027,
    .subject = 69,
    .data =
    {
      .i32 = { 0xCAFEBABE, 0xDEADBEEF, 0x1BADD00D, 0xC0FFEE, 0xF00DFACE }
    }
  };

  sys_wait(500);

  if(sys_send(&msg, 1) == ESYS_OK)
    return 0;
  else
    return 1;
}
