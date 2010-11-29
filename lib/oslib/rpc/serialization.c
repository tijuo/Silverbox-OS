#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "serialization.h"

static void _free_rpc_node(struct RPC_Node *node);
static enum RPC_Error _parse_null(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_true(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_false(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_byte(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_int16(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_int32(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_int64(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_float(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_uuid(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_string(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_array(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_hash(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_pair(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_key(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_value(char **str, struct RPC_Node *node);
static enum RPC_Error _parse_node(char **str, struct RPC_Node *node);

struct RPC_Node *parse_rpc_string(char *str)
{
  enum RPC_Error status;
  struct RPC_Node *node = calloc(1, sizeof *node);

  if( !node )
    return NULL;

  status = _parse_node(&str, node);

  if( status != RPC_OK )
  {
    free(node);
    return NULL;
  }
  else
    return node;
}

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

static void _free_rpc_node(struct RPC_Node *node)
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

static enum RPC_Error _parse_null(char **str, struct RPC_Node *node)
{
  if( **str != RPC_NULL )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_NULL;
  node->children = NULL;
  (*str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_true(char **str, struct RPC_Node *node)
{
  if( **str != RPC_TRUE )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_TRUE;
  node->children = NULL;
  (*str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_false(char **str, struct RPC_Node *node)
{
  if( **str != RPC_FALSE )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_FALSE;
  node->children = NULL;
  (*str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_byte(char **str, struct RPC_Node *node)
{
  if( **str != RPC_BYTE )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_BYTE;
  node->children = NULL;
  node->data.b = *(++(*str));
  (*str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_int16(char **str, struct RPC_Node *node)
{
  if( **str != RPC_INT16 )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_INT16;
  node->children = NULL;
  node->data.i16 = *((int16_t *)++*str);
  (*(int16_t **)str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_int32(char **str, struct RPC_Node *node)
{
  if( **str != RPC_INT32 )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_INT32;
  node->children = NULL;
  node->data.i32 = *((int32_t *)++*str);
  (*(int32_t **)str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_int64(char **str, struct RPC_Node *node)
{
  if( **str != RPC_INT64 )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_INT64;
  node->children = NULL;
  node->data.i64 = *((int64_t *)++*str);
  (*(int64_t **)str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_float(char **str, struct RPC_Node *node)
{
  if( **str != RPC_FLOAT )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_FLOAT;
  node->children = NULL;
  node->data.f = *((double *)++*str);
  (*(double **)str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_uuid(char **str, struct RPC_Node *node)
{
  if( **str != RPC_UUID )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_UUID;
  node->children = NULL;
  node->data.uuid = *((uuid_t *)++*str);
  (*(uuid_t **)str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_string(char **str, struct RPC_Node *node)
{
  unsigned long long length;

  if( (**str & 0xF0) != RPC_STRING )
    return RPC_PARSE_ERROR;

  switch( **str & 0xF )
  {
    case RPC_NULL:
      node->data.string.seq = NULL;
      length = 0;
      break;
    case RPC_BYTE:
      length = *(int8_t *)(++(*str));
      ++(*str);
      break;
    case RPC_UINT16:
      length = *(uint16_t *)(++(*str));
      ++(*(uint16_t **)str);
      break;
    case RPC_UINT32:
      length = *(uint32_t *)(++(*str));
      ++(*(uint32_t **)str);
      break;
    case RPC_UINT64:
      length = *(uint64_t *)(++(*str));
      ++(*(uint64_t **)str);
      break;
    default:
      return RPC_PARSE_ERROR;
  }

  node->data.string.len = length;

  if( length )
  {
    node->data.string.seq = malloc(length);

    if( !node->data.string.seq )
      return RPC_FAIL;

    // XXX: Need to add support for a 64-bit memcpy()

    memcpy(node->data.string.seq, *str, (size_t)length);
    *str += length;
  }

  node->numChildren = 0;
  node->type = RPC_STRING;
  node->children = NULL;

  return RPC_OK;
}

static enum RPC_Error _parse_array(char **str, struct RPC_Node *node)
{
  unsigned long long numElements=0;
  enum RPC_Error status;

  if( (**str & 0xF0) != RPC_ARRAY )
    return RPC_PARSE_ERROR;

  switch( **str & 0xF )
  {
    case RPC_NULL:
      numElements = 0;
      break;
    case RPC_BYTE:
      numElements = *(uint8_t *)(++(*str));
      ++(*str);
      break;
    case RPC_UINT16:
      numElements = *(uint16_t *)(++(*str));
      ++(*(uint16_t **)str);
      break;
    case RPC_UINT32:
      numElements = *(uint32_t *)(++(*str));
      ++(*(uint32_t **)str);
      break;
    case RPC_UINT64:
      numElements = *(uint64_t *)(++(*str));
      ++(*(uint64_t **)str);
      break;
    default:
      return RPC_PARSE_ERROR;
  }

  if( numElements )
  {
    if( (node->children = calloc(numElements, sizeof *node)) == NULL )
      return RPC_FAIL;

    for( unsigned long long i=0; i < numElements; i++ )
    {
      if( (status=_parse_node(str, &node->children[i])) != RPC_OK )
      {
        while( i )
          _free_rpc_node(&node->children[i-- - 1]);

        free(node->children);
        return status;
      }
    }
  }

  node->numChildren = numElements;
  node->type = RPC_ARRAY;

  return RPC_OK;
}

static enum RPC_Error _parse_hash(char **str, struct RPC_Node *node)
{
  unsigned long long numPairs=0;
  enum RPC_Error status;

  if( (**str & 0xF0) != RPC_HASH )
    return RPC_PARSE_ERROR;

  switch( **str & 0xF )
  {
    case RPC_NULL:
      numPairs = 0;
      break;
    case RPC_BYTE:
      numPairs = *(uint8_t *)(++(*str));
      ++(*str);
      break;
    case RPC_UINT16:
      numPairs = *(uint16_t *)(++(*str));
      ++(*(uint16_t **)str);
      break;
    case RPC_UINT32:
      numPairs = *(uint32_t *)(++(*str));
      ++(*(uint32_t **)str);
      break;
    case RPC_UINT64:
      numPairs = *(uint64_t *)(++(*str));
      ++(*(uint64_t **)str);
      break;
    default:
      return RPC_PARSE_ERROR;
  }

  if( numPairs )
  {
    if( (node->children = calloc(numPairs, sizeof *node)) == NULL )
      return RPC_FAIL;

    for( unsigned long long i=0; i < numPairs; i++ )
    {
      if( (status=_parse_pair(str, &node->children[i])) != RPC_OK )
      {
        while( i )
          _free_rpc_node(&node->children[i-- - 1]);

        free(node->children);
        return status;
      }
    }
  }

  node->numChildren = numPairs;
  node->type = RPC_HASH;

  return RPC_OK;
}

static enum RPC_Error _parse_pair(char **str, struct RPC_Node *node)
{
  enum RPC_Error status;

  if( (node->children = calloc(2, sizeof *node)) == NULL )
    return RPC_FAIL;

  if( (status=_parse_key(str,&node->children[0])) != RPC_OK )
  {
    free(node->children);
    return status;
  }

  if( (status=_parse_value(str, &node->children[1])) != RPC_OK )
  {
    _free_rpc_node(&node->children[0]);
    free(node->children);
    return status;
  }

  node->numChildren = 2;
  node->type = RPC_PAIR;

  return RPC_OK;
}

static enum RPC_Error _parse_key(char **str, struct RPC_Node *node)
{
  enum RPC_Error status;

  if( (node->children = calloc(1, sizeof *node)) == NULL )
    return RPC_FAIL;

  if( (status=_parse_node(str,&node->children[0])) != RPC_OK )
  {
    free(node->children);
    return status;
  }

  node->numChildren = 1;
  node->type = RPC_KEY;

  return RPC_OK;
}

static enum RPC_Error _parse_value(char **str, struct RPC_Node *node)
{
  enum RPC_Error status;

  if( (node->children = calloc(1, sizeof *node)) == NULL )
    return RPC_FAIL;

  if( (status=_parse_node(str,&node->children[0])) != RPC_OK )
  {
    free(node->children);
    return status;
  }

  node->numChildren = 1;
  node->type = RPC_VALUE;

  return RPC_OK;
}

static enum RPC_Error _parse_node(char **str, struct RPC_Node *node)
{
  switch( **str )
  {
    case RPC_NULL:
      return _parse_null(str,node);
    case RPC_TRUE:
      return _parse_true(str,node);
    case RPC_FALSE:
      return _parse_false(str,node);
    case RPC_BYTE:
      return _parse_byte(str,node);
    case RPC_INT16:
      return _parse_int16(str,node);
    case RPC_INT32:
      return _parse_int32(str,node);
    case RPC_INT64:
      return _parse_int64(str,node);
    case RPC_UUID:
      return _parse_uuid(str,node);
    case RPC_FLOAT:
      return _parse_float(str,node);
    default:
      break;
  }

  switch( **str & 0xF0 )
  {
    case RPC_STRING:
      return _parse_string(str,node);
    case RPC_ARRAY:
      return _parse_array(str,node);
    case RPC_HASH:
      return _parse_hash(str,node);
    default:
      break;
  }

  return RPC_PARSE_ERROR;
}
