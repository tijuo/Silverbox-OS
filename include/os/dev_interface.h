#ifndef DEVICE_INTERFACE
#define DEVICE_INTERFACE

#define DEVICE_READ  		0
#define DEVICE_WRITE 		1
#define DEVICE_IOCTL 		2

#include <oslib.h>

#define MAJOR(x)		(((x) >> 8) & 0xFF)
#define MINOR(x)		((x) & 0xFF)

#define DEVICE_REQUEST		0
#define DEVICE_RESPONSE		128
#define DEVICE_ERROR		64
#define DEVICE_MORE		32
#define DEVICE_NOMORE		0
#define DEVICE_SUCCESS		0

int deviceRead(tid_t tid, unsigned char device, unsigned offset,
                size_t num_blks, shmid_t shmid, size_t *blocks_read);
int deviceWrite(tid_t tid, unsigned char device, unsigned offset,
                size_t num_blks, shmid_t shmid, size_t *blocks_written);
int deviceIoctl(tid_t tid, unsigned char device, short int command,
                void *in_buffer, size_t args_len, void *out_buffer);

#endif /* DEVICE_INTERFACE */
