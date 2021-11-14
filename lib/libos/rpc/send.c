#include <os/rpc/rpc.h>
#include <os/message.h>

#define MSG_TIMEOUT	3000

int sendRPCMessage(tid_t recipient, char *name, uchar *message, uint64_t msgLen,
                   int protocol)
{
  int result = -1;

  switch(protocol) {
    case RPC_PROTO_RPC:
      result = sendRPC(recipient, name, message, msgLen);
      break;
    case RPC_PROTO_JSON:
      result = sendRPC(recipient, name, message, msgLen);
      break;
    case RPC_PROTO_BSON: // Not implemented yet
      result = -1;
      break;
    default:
      result = -1;
      break;
  }
}

int _sendRPC(tid_t recipient, char *name, uchar *message, uint64_t msgLen,
             int protocol)
{
  volatile struct Message msg;
  volatile struct RpcHeader *header;
  char *buffer;
  int result;

  if(!name || (!header && msgLen))
    return -1;

  header = (volatile struct *)msg.data;
  header->protocol = protocol;
  header->nameLen = strlen(name);
  header->dataLen = msgLen;

  msg.length = sizeof *header;
  msg.protocol = MSG_PROTO_RPC;

  if(sendMsg(recipient, &msg, MSG_TIMEOUT) < 0)
    return -1;

  buffer = malloc(header->dataLen + header->nameLen);

  if(!buffer)
    return -1;

  memcpy(buffer, name, header->nameLen);
  memcpy(buffer + header->nameLen, message, msgLen);

  sendLong(recipient, buffer, 0);

  free(buffer);

  return 0;
}

int sendBSON(tid_t recipient, char *name, uchar *message, uint64_t msgLen) {
  return -1;
  /*return _sendRPC(recipient, message, msgLen, RPC_PROTO_BSON);*/
}

int sendJSON(tid_t recipient, char *name, uchar *message, uint64_t msgLen) {
  return _sendRPC(recipient, message, msgLen, RPC_PROTO_JSON);
}

int sendRPC(tid_t recipient, char *name, uchar *message, uint64_t msgLen) {
  return _sendRPC(recipient, message, msgLen, RPC_PROTO_RPC);
}
