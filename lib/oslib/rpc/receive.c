#include <os/rpc/rpc.h>
#include <os/message.h>

int receiveRPC(struct Message *msg, char **fName, struct RPC_Node **node)
{
  struct RpcHeader *header = (struct RpcHeader *)msg->data;
  void *buffer;
  uchar *rpcStr;

  buffer = malloc(header->nameLen+header->dataLen);

  if( !buffer )
    return -1;

  if( _receive(msg->sender, buffer, header->nameLen+header->dataLen, 0) != 0 )
  {
    free(buffer);
    return -1;
  }

  if( fName )
  {
    *fName = malloc(header->nameLen+1);

    if( !*fName )
    {
      free(buffer);
      return -1;
    }
    strncpy(*fName, (char *)buffer, header->nameLen);
    *fName[header->nameLen] = '\0';
  }

  if( node )
  {
    switch( header->protocol )
    {
      case RPC_PROTO_RPC:
        *node = parse_rpc_string((uchar *)buffer + header->nameLen);
        break;
      case RPC_PROTO_JSON:
        *node = parseJSON((uchar *)buffer + header->nameLen);
        break;
      default:
        free(buffer);
        free(*fName);
        break;
    }
  }

  free(buffer);

  return 0;
}
