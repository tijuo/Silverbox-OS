#include <stddef.h>

struct RPC_Node *parse_rpc_string(char *str)
{
  enum RPC_Error status;
  struct RPC_Node *node = calloc(1, sizeof *node);

  if( !node )
    return NULL;
  
  status = _parse_node(str, node);

  if( status != RPC_OK )
  {
    free(node);
    return NULL;
  }
  else
    return node;
}

void free_rpc_node(struct RPC_Node *node)
{
  if( !node )
    return;

  switch(node->type)
  {
    case RPC_STRING:
      free(node->seq);
      break;
    case RPC_ARRAY:
    case RPC_HASH:
    case RPC_PAIR:
    case RPC_KEY:
    case RPC_VALUE:
      while(node->numChildren)
        free_rpc_node(&node->children[--node->numChildren]);
      free(node->children);
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
  node->data.i16 = *((int16 *)++*str);
  ((int16 *)*str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_int32(char **str, struct RPC_Node *node)
{
  if( **str != RPC_INT32 )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_INT32;
  node->children = NULL;
  node->data.i32 = *((int32 *)++*str);
  ((int32 *)*str)++;

  return RPC_OK;
}

static enum RPC_Error _parse_int64(char **str, struct RPC_Node *node)
{
  if( **str != RPC_INT64 )
    return RPC_PARSE_ERROR;

  node->numChildren = 0;
  node->type = RPC_INT64;
  node->children = NULL;
  node->data.i64 = *((int64 *)++*str);
  ((int64 *)*str)++;

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
  ((uuid_t *)*str)++;

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
      node->data.seq = NULL;
      length = 0;
      break;
    case RPC_BYTE:
      length = *(uchar8 *)(++(*str));
      ++(*str);
      break;
    case RPC_UINT16:
      length = *(uint16 *)(++(*str));
      ++((uint16 *)*str);
      break;
    case RPC_UINT32:
      length = *(uint32 *)(++(*str));
      ++((uint32 *)*str);
      break;
    case RPC_UINT64:
      length = *(uint64 *)(++(*str));
      ++((uint64 *)*str);
      break;
    default:
      return RPC_PARSE_ERROR;
  }

  node->data.len = length;

  if( length )
  {
    node->data.seq = malloc(length);

    if( !node->data.seq )
      return RPC_FAIL;

    // XXX: Need to add support for a 64-bit memcpy()

    memcpy(node->data.seq, *str, (size_t)length);
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
      numElements = *(uchar8 *)(++(*str));
      ++(*str);
      break;
    case RPC_UINT16:
      numElements = *(uint16 *)(++(*str));
      ++((uint16 *)*str);
      break;
    case RPC_UINT32:
      numElements = *(uint32 *)(++(*str));
      ++((uint32 *)*str);
      break;
    case RPC_UINT64:
      numElements = *(uint64 *)(++(*str));
      ++((uint64 *)*str);
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
          free_rpc_node(&node->children[i-- - 1]);

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

  if( (**str & 0xF0) != RPC_ARRAY )
    return RPC_PARSE_ERROR;

  switch( **str & 0xF )
  {
    case RPC_NULL:
      numPairs = 0;
      break;
    case RPC_BYTE:
      numPairs = *(uchar8 *)(++(*str));
      ++(*str);
      break;
    case RPC_UINT16:
      numPairs = *(uint16 *)(++(*str));
      ++((uint16 *)*str);
      break;
    case RPC_UINT32:
      numPairs = *(uint32 *)(++(*str));
      ++((uint32 *)*str);
      break;
    case RPC_UINT64:
      numPairs = *(uint64 *)(++(*str));
      ++((uint64 *)*str);
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
          free_rpc_node(&node->children[i-- - 1]);

        free(node->children);
        return status;
      }
    }
  }

  node->numChildren = numPair;
  node->type = RPC_ARRAY;

  return RPC_OK;
}

static enum RPC_Error _parse_pair(char **str, struct RPC_Node *node)
{
  enum RPC_Error status;

  if( (node->children = calloc(2, sizeof *node) == NULL )
    return RPC_FAIL;

  if( (status=_parse_key(str,&node->children[0])) != RPC_OK )
  {
    free(node->children);
    return status;
  }

  if( (status=_parse_value(str, &node->children[1])) != RPC_OK )
  {
    free_rpc_node(&node->children[0]);
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

  if( (node->children = calloc(1, sizeof *node) == NULL )
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

  if( (node->children = calloc(1, sizeof *node) == NULL )
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
