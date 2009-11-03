#ifndef DEVICE_INTERFACE
#define DEVICE_INTERFACE

#define DEVICE_READ  		0
#define DEVICE_WRITE 		1
#define DEVICE_IOCTL 		2

#include <oslib.h>

#define MAJOR(x)		((x >> 8) & 0xFF)
#define MINOR(x)		(x & 0xFF)

#define DEVICE_REQUEST		0
#define DEVICE_RESPONSE		128
#define DEVICE_ERROR		64
#define DEVICE_MORE		32
#define DEVICE_NOMORE		0
#define DEVICE_SUCCESS		0

struct DeviceMsg
{
  char msg_type;
  unsigned char deviceNum;

  union
  {
    size_t length;
    size_t count;
    size_t blocks;
    size_t args_len;
  };

  union
  {
    unsigned offset;
    unsigned command;
  };
  unsigned char data[];
};

int deviceRead( tid_t tid, unsigned char device, unsigned offset, size_t num_blks,
                size_t blk_len, void *buffer );
int deviceWrite( tid_t tid, unsigned char device, unsigned offset, size_t num_blks, size_t blk_size, void *buffer );
int deviceIoctl( tid_t tid, unsigned char device, int command, void *args, size_t args_len);

#endif /* DEVICE_INTERFACE */
