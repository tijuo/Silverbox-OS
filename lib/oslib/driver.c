#include <oslib.h>
#include <os/dev_interface.h>
#include <string.h>
#include <os/message.h>

#define MSG_TIMEOUT	15000

int deviceRead(pid_t pid, unsigned char device, unsigned offset,
                size_t num_blks, shmid_t shmid, size_t *blocks_read)
{
  int in_args[5] = { DEVICE_READ, device, num_blocks, offset, shmid };
  int out_args[5];

  int result = sys_rpc(pid, in_args, out_args, MSG_TIMEOUT);

  if(result < 0)
    return result;
  else
  {
    if(blocks_read)
      *blocks_read = (size_t)out_args[0];

    return 0;
  }
}

int deviceWrite(tid_t tid, unsigned char device, unsigned offset,
                size_t num_blks, shmid_t shmid, size_t *blocks_written)
{
  int in_args[4] = { (int)(device << 8) | DEVICE_WRITE,
                  num_blocks, offset, shmid };
  int out_args[4];

  int result = sys_rpc(tid, in_args, out_args, MSG_TIMEOUT);

  if(result < 0)
    return result;
  else
  {
    if(blocks_written)
      *blocks_written = (size_t)out_args[0];

    return 0;
  }
}

int deviceIoctl(tid_t tid, unsigned char device, short int command, 
                void *in_buffer, size_t args_len, void *out_buffer)
{
  int in_args[4] = { (int)(command<<16) | (int)(device<<8) | DEVICE_IOCTL,
                     0, 0, 0 };
  if(args_len <= 12)
    memcpy(&in_args[1], in_buffer, args_len);

  return sys_rpc(tid, in_args, (int *)out_buffer, MSG_TIMEOUT);
}
