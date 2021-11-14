#include <oslib.h>
#include <os/dev_interface.h>
#include <string.h>
#include <os/message.h>
#include <os/msg/message.h>
#include <os/syscalls.h>
#include <stdlib.h>

int deviceRead(tid_t tid, struct DeviceOpRequest *request, void *buffer,
               size_t *blocksRead)
{
  if(!request)
    return -1;

  msg_t requestMsg = {
    .recipient = tid,
    .subject = DEVICE_READ,
    .buffer = request,
    .bufferLen = sizeof *request,
    .flags = 0,
  };

  msg_t responseMsg = EMPTY_MSG
  ;

  responseMsg.buffer = buffer;
  responseMsg.bufferLen = request->length;

  int ret = sys_call(&requestMsg, &responseMsg);

  if(blocksRead && ret == ESYS_OK && responseMsg.subject == RESPONSE_OK) {
    if(blocksRead)
      *blocksRead = responseMsg.bytesTransferred;

    return 0;
  }
  else
    return -1;
}

int deviceWrite(tid_t tid, struct DeviceOpRequest *request,
                size_t *blocksWritten)
{
  if(!request)
    return -1;

  msg_t requestMsg = {
    .recipient = tid,
    .subject = DEVICE_WRITE,
    .buffer = request,
    .bufferLen = sizeof *request + request->length,
    .flags = 0,
  };

  msg_t responseMsg = EMPTY_MSG
  ;

  responseMsg.buffer = blocksWritten;
  responseMsg.bufferLen = blocksWritten ? sizeof(size_t) : 0;

  int ret = sys_call(&requestMsg, &responseMsg);

  if(ret == ESYS_OK && responseMsg.subject == RESPONSE_OK) {
    if(blocksWritten)
      *blocksWritten = responseMsg.bytesTransferred;
    return 0;
  }

  return -1;
}

/*
 int deviceIoctl(tid_t tid, unsigned char device, short int command, 
 void *in_buffer, size_t args_len, void *out_buffer)
 {
 int in_args[4] = { (int)(command<<16) | (int)(device<<8) | DEVICE_IOCTL,
 0, 0, 0 };
 if(args_len <= 12)
 memcpy(&in_args[1], in_buffer, args_len);

 return sys_rpc(tid, in_args, (int *)out_buffer, MSG_TIMEOUT);
 }
 */
