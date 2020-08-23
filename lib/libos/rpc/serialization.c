#include "serialization.h"
#include <stdlib.h>
#include <string.h>

void _free_rpc_node(struct RPC_Node *node);

struct RPC_Node *rpc_new_node(void)
{
  struct RPC_Node *node = malloc(sizeof *node);

  rpc_null(node);
  return node;
}

int rpc_insert_child(struct RPC_Node *node, unsigned long long pos,
  struct RPC_Node *parent)
{
  struct RPC_Node *newChildren;

  if( !parent || !node )
    return 0;

  if( pos > parent->numChildren )
    pos = parent->numChildren;

  newChildren = realloc(parent->children,
                        (parent->numChildren + 1) * sizeof *node);

  if( !newChildren )
    return 0;

  for(unsigned long long i=parent->numChildren; i < pos; i--)
    newChildren[i] = newChildren[i-1];

  newChildren[pos] = *node;
  parent->numChildren++;
  parent->children = newChildren;

  free(node);

  return 1;
}

int rpc_append_child(struct RPC_Node *node, struct RPC_Node *parent)
{
  if( !parent )
    return 0;

  return rpc_insert_child(node, parent->numChildren, parent);
}

void rpc_delete_node(struct RPC_Node *node)
{
  if( !node )
    return;

  _free_rpc_node(node);
  free(node);
}

void rpc_remove_children(struct RPC_Node *node)
{
  if( !node )
    return;

  while(node->numChildren)
    _free_rpc_node(&node->children[--node->numChildren]);

  free(node->children);
}

int rpc_null(struct RPC_Node *node)
{
  if( !node )
    return 0;

  node->numChildren = 0;
  node->type = RPC_NULL;
  node->children = NULL;

  return 1;
}

int rpc_float(double f, struct RPC_Node *node)
{
  if( !rpc_null(node) )
    return 0;

  node->type = RPC_FLOAT;
  node->data.f = f;

  return 1;
}

int rpc_bool(bool b, struct RPC_Node *node)
{
  if( rpc_null(node) )
  {
    node->type = b ? RPC_TRUE : RPC_FALSE;
    return 1;
  }

  return 0;
}

int rpc_array(struct RPC_Node *node)
{
  if( !rpc_null(node) )
    return 0;

  node->type = RPC_ARRAY;

  return 1;
}

int rpc_hash(struct RPC_Node *node)
{
  if( !rpc_null(node) )
    return 0;

  node->type = RPC_HASH;

  return 1;
}

int rpc_byte(int8_t b, struct RPC_Node *node)
{
  if( !rpc_null(node) )
    return 0;

  node->type = RPC_BYTE;
  node->data.b = b;

  return 1;
}

int rpc_int16(int16_t i, struct RPC_Node *node)
{
  if( !rpc_null(node) )
    return 0;

  node->type = RPC_INT16;
  node->data.i16 = i;

  return 1;
}

int rpc_int32(int32_t i, struct RPC_Node *node)
{
  if( !rpc_null(node) )
    return 0;

  node->type = RPC_INT32;
  node->data.i32 = i;

  return 1;
}

int rpc_int64(int64_t i, struct RPC_Node *node)
{
  if( !rpc_null(node) )
    return 0;

  node->type = RPC_INT64;
  node->data.i64 = i;

  return 1;
}

int rpc_uuid(uuid_t u, struct RPC_Node *node)
{
  if( !rpc_null(node) )
    return 0;

  node->type = RPC_UUID;
  node->data.uuid = u;

  return 1;
}

int rpc_string(void *seq, size_t len, struct RPC_Node *node)
{
  if( !rpc_null(node) )
    return 0;

  node->type = RPC_STRING;

  if( (node->data.string.seq = malloc(len)) == NULL )
    return 0;

  memcpy(node->data.string.seq, seq, len);
  node->data.string.len = len;

  return 1;
}

void _free_rpc_node(struct RPC_Node *node)
{
  if( !node )
    return;

  switch(node->type)
  {
    case RPC_STRING:
      free(node->data.string.seq);
      break;
    case RPC_ARRAY:
    case RPC_HASH:
    case RPC_PAIR:
    case RPC_KEY:
    case RPC_VALUE:
      rpc_remove_children(node);
      break;
    default:
      break;
  }
}
