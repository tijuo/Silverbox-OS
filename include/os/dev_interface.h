#ifndef DEVICE_INTERFACE
#define DEVICE_INTERFACE

#define DEVICE_READ  		0
#define DEVICE_WRITE 		1
#define DEVICE_IOCTL 		2

#include <oslib.h>

#define MAJOR(x)		(unsigned short int)(((x) >> 16) & 0xFFFF)
#define MINOR(x)		(unsigned short int)((x) & 0xFFFF)

#define DEVICE_REQUEST		0
#define DEVICE_RESPONSE		128
#define DEVICE_ERROR		64
#define DEVICE_MORE		    32
#define DEVICE_NOMORE		16
#define DEVICE_SUCCESS		0

#define DEVF_APPEND         1       // ignore offset for writes. writes will occur at the end of the device

struct DeviceOpRequest {
  unsigned short deviceMinor;
  uint64_t offset;
  size_t length;
  int flags;
  unsigned char payload[];
};

struct DeviceWriteResponse {
  size_t bytesTransferred;
};

struct DeviceIoctlRequest {
  unsigned short deviceMinor;
  int command;
  unsigned char data[];
};

int deviceRead(tid_t tid, struct DeviceOpRequest *request, void *buffer, size_t *blocks_read);
int deviceWrite(tid_t tid, struct DeviceOpRequest *request, size_t *blocks_written);
int deviceIoctl(tid_t tid, unsigned short device, short int command,
                void *in_buffer, size_t args_len, void *out_buffer);

#endif /* DEVICE_INTERFACE */
