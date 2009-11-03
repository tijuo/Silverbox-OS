#include <os/message.h>

void _init_out_queue(void)
{
  __out_pointer = __out_tail = 0;

  __out_queue.queueAddr = __out_msgs;
  __out_queue.length = sizeof __out_msgs / sizeof(struct Message);
  __out_queue.pointer = &__out_pointer;
  __out_queue.tail = &__out_tail;

  __set_out_queue( &__out_queue );
}
