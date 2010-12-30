#ifndef RPC_H
#define RPC_H

#include <serialization.h>

#define RPC_PROTO_RPC		1
#define RPC_PROTO_JSON		2
#define RPC_PROTO_BSON		3

struct RpcHeader
{
  int protocol;
  uuid_t tag;
  uint64_t dataLen;
  uchar data[];
};

#endif /* RPC_H */
