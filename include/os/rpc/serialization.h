#ifndef RPC_SERIALIZATION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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
#define RPC_FLOAT		8

#define RPC_ARRAY		0x20		// A sequence of nodes
#define RPC_STRING		0x30		// A sequence of bytes
#define RPC_HASH		0x40		// An array of pairs
#define RPC_CALL		0x50
#define RPC_RESPONSE		0x60

#define RPC_PAIR		0x100
#define	RPC_KEY			0x101
#define RPC_VALUE		0x102

typedef struct { int32_t n[4]; } uuid_t;
typedef uint8_t uchar;

struct RPC_Node
{
  int type;
  struct RPC_Node *children;
  unsigned long long numChildren;

  union RPC_Data
  {
    int8_t b;
    double f;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uuid_t uuid;

    struct RPC_String_Data
    {
      int8_t *seq;
      unsigned long long len;
    } string;
  } data;
};

struct RPC_Message
{
  int type; // Either a call or response
  uuid_t tag;
  struct RPC_Node functionName; // A string
  struct RPC_Node arguments; // Either an array, hash, or null
};

enum RPC_Error { RPC_OK, RPC_PARSE_ERROR, RPC_FAIL };

struct RPC_Node *parse_rpc_string(uchar *str);
int rpc_to_string(struct RPC_Node *node, uchar **str, uint64_t *len);

struct RPC_Node *rpc_new_node(void);
int rpc_insert_child(struct RPC_Node *node, unsigned long long pos, 
  struct RPC_Node *parent);
int rpc_append_child(struct RPC_Node *node, struct RPC_Node *parent);
void rpc_delete_node(struct RPC_Node *node);
void rpc_remove_children(struct RPC_Node *node);

int rpc_null(struct RPC_Node *node);
int rpc_bool(bool b, struct RPC_Node *node);
int rpc_float(double f, struct RPC_Node *node);
int rpc_array(struct RPC_Node *node);
int rpc_hash(struct RPC_Node *node);
int rpc_byte(int8_t b, struct RPC_Node *node);
int rpc_int16(int16_t i, struct RPC_Node *node);
int rpc_int32(int32_t i, struct RPC_Node *node);
int rpc_int64(int64_t i, struct RPC_Node *node);
int rpc_uuid(uuid_t u, struct RPC_Node *node);
int rpc_string(void *seq, size_t len, struct RPC_Node *node);

#endif /* RPC_SERIALIZATION_H */
