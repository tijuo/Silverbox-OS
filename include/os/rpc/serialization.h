#ifndef RPC_SERIALIZATION_H

#include <stddef.h>

#define RPC_PROTOCOL		1

#define RPC_NULL		0
#define RPC_TRUE		1
#define RPC_FALSE		2
#define RPC_CHAR		3
#define RPC_BYTE		RPC_CHAR
#define	RPC_INT16		4
#define RPC_UINT16		RPC_INT16
#define RPC_INT32		5
#define RPC_UINT32		RPC_INT32
#define RPC_INT64		6
#define RPC_UINT64		RPC_INT64
#define RPC_UUID		7

#define RPC_ARRAY		0x20		// A sequence of nodes
#define RPC_STRING		0x30		// A sequence of bytes
#define RPC_HASH		0x40		// An array of pairs
#define RPC_CALL		0x50
#define RPC_RESPONSE		0x60

#define RPC_PAIR		0x100
#define	RPC_KEY			0x101
#define RPC_VALUE		0x102

typedef struct { int32 n[4]; } uuid_t;

struct RPC_Message
{
  int type; // Either a call or response
  uuid_t tag;
  struct RPC_Node functionName; // A string
  struct RPC_Node arguments; // Either an array, hash, or null
};

struct RPC_Node
{
  int type;
  struct RPC_Node *children;
  unsigned long long numChildren;

  union RPC_Data
  {
    char8 b;
    int16 i16;
    int32 i32;
    int64 i64;
    uuid_t uuid;

    struct
    {
      char8 *seq;
      unsigned long long len;
    };
  } data;
};

enum RPC_Error { RPC_OK, RPC_PARSE_ERROR, RPC_FAIL };

struct RPC_Node *parse_rpc_string(char *str);
void free_rpc_node(struct RPC_Node *node);

#endif /* RPC_SERIALIZATION_H */
