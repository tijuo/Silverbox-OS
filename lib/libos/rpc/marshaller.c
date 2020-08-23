#include "serialization.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>

static int _onebyte_string(struct RPC_Node *node, uchar val, uchar **str, uint64_t *len);
static int _null_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _true_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _false_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _byte_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _int16_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _int32_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _int64_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _uuid_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _sequence_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _array_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _hash_string(struct RPC_Node *node, uchar **str, uint64_t *len);
static int _node_string(struct RPC_Node *node, uchar **str, uint64_t *len);

int rpc_to_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  if( !node || !str || !len )
    return 0;
  else
    return _node_string(node, str, len);
}

static int _onebyte_string(struct RPC_Node *node, uchar val, uchar **str, uint64_t *len)
{
  if( node->numChildren != 0 )
    return 0;

  *str = malloc(1);

  if( !*str )
    return 0;

  **str = val;
  *len = 1;

  return 1;
}

static int _null_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  return _onebyte_string(node, (uchar)RPC_NULL, str, len);
}

static int _true_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  return _onebyte_string(node, (uchar)RPC_TRUE, str, len);
}

static int _false_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  return _onebyte_string(node, (uchar)RPC_FALSE, str, len);
}

static int _byte_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  if( node->numChildren != 0 )
    return 0;

  *str = malloc(2);

  if( !*str )
    return 0;

  *str[0] = (uchar)RPC_CHAR;
  *str[1] = (uchar)node->data.b;
  *len = 2;

  return 1;
}

static int _int16_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  if( node->numChildren != 0 )
    return 0;

  *str = malloc(3);

  if( !*str )
    return 0;

  *str[0] = (uchar)RPC_INT16;
  *(int16_t *)(str + 1) = node->data.i16;
  *len = 3;

  return 1;
}

static int _int32_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  if( node->numChildren != 0 )
    return 0;

  *str = malloc(5);

  if( !*str )
    return 0;

  *str[0] = (uchar)RPC_INT32;
  *(int32_t *)(*str + 1) = node->data.i32;
  *len = 5;

  return 1;
}

static int _int64_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  if( node->numChildren != 0 )
    return 0;

  *str = malloc(9);

  if( !*str )
    return 0;

  *str[0] = (uchar)RPC_INT64;
  *(int64_t *)(*str + 1) = node->data.i64;
  *len = 9;

  return 1;
}

static int _float_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  if( node->numChildren != 0 )
    return 0;

  *str = malloc(9);

  if( !*str )
    return 0;

  *str[0] = (uchar)RPC_FLOAT;
  *(double *)(*str + 1) = node->data.f;
  *len = 9;

  return 1;
}

static int _uuid_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  if( node->numChildren != 0 )
    return 0;

  *str = malloc(17);

  if( !*str )
    return 0;

  *str[0] = (uchar)RPC_UUID;
  *(uuid_t *)(*str + 1) = node->data.uuid;
  *len = 17;

  return 1;
}

#define _PREPARE_STR(COUNT, SIZE_TYPE) \
  if( COUNT == 0 ) \
    SIZE_TYPE = RPC_NULL; \
  else if( COUNT <= UCHAR_MAX ) \
    SIZE_TYPE = RPC_BYTE; \
  else if( COUNT <= USHRT_MAX ) \
    SIZE_TYPE = RPC_UINT16; \
  else if( COUNT <= UINT_MAX ) \
    SIZE_TYPE = RPC_UINT32; \
  else \
    SIZE_TYPE = RPC_UINT64; \
\
  switch( SIZE_TYPE ) \
  { \
    case RPC_UINT64: \
      *len += 4; \
    case RPC_UINT32: \
      *len += 2; \
    case RPC_UINT16: \
      *len += 1; \
    case RPC_BYTE: \
      *len += 1; \
    case RPC_NULL: \
    default: \
      break; \
  } \
  *len += 1;

#define _BUILD_STR(TYPE, COUNT, SIZE_TYPE) \
  *str[0] = TYPE | SIZE_TYPE; \
\
  switch( SIZE_TYPE ) \
  { \
    case RPC_UINT64: \
      *(uint64_t *)(*str+1) = (uint64_t)COUNT; \
      break; \
    case RPC_UINT32: \
      *(uint32_t *)(*str+1) = (uint32_t)COUNT; \
      break; \
    case RPC_UINT16: \
      *(uint16_t *)(*str+1) = (uint16_t)COUNT; \
      break; \
    case RPC_BYTE: \
      *(uint8_t *)(*str+1) = (uint8_t)COUNT; \
      break; \
    case RPC_NULL: \
    default: \
      break; \
  }

static int _sequence_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  uchar size_type;
  *len = 0;

  if( node->numChildren != 0 )
    return 0;

  _PREPARE_STR(node->data.string.len, size_type);

  *len += node->data.string.len;
  *str = malloc(*len);

  if( !*str )
    return 0;

  _BUILD_STR(RPC_STRING, node->data.string.len, size_type)

  // XXX: Need a 64-bit memcpy

  memcpy(*str + (*len - node->data.string.len), node->data.string.seq, 
         (size_t)node->data.string.len);

  return 1;
}

static int _array_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  uchar size_type, *element_str, *new_str;
  uint64_t element_len;
  *len = 0;

  _PREPARE_STR(node->numChildren, size_type)

  *str = malloc(*len);

  if( !*str )
    return 0;

  _BUILD_STR(RPC_ARRAY, node->numChildren, size_type)

  for(unsigned long long i=0; i < node->numChildren; i++)
  {
    if( _node_string(&node->children[i], &element_str, &element_len) == 0 )
    {
      free(*str);
      return 0;
    }

    // XXX: Need a 64-bit realloc
    new_str = realloc(*str, (size_t)*len + (size_t)element_len);

    if( !new_str )
    {
      free(element_str);
      free(*str);
      return 0;
    }

    *str = new_str;

    //XXX: Need a 64-bit memcpy

    memcpy(*str + *len, element_str, (size_t)element_len);
    *len += element_len;
    free(element_str);
  }

  return 1;
}

static int _hash_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  uchar size_type, *element_str, *new_str;
  uint64_t element_len;
  struct RPC_Node *key, *value, *pair;

  *len = 0;

  _PREPARE_STR(node->numChildren, size_type)

  *str = malloc(*len);

  if( !*str )
    return 0;

  _BUILD_STR(RPC_HASH, node->numChildren, size_type)

  for(unsigned long long i=0; i < node->numChildren; i++)
  {
    pair = &node->children[i];

    if( pair->numChildren != 2 )
      goto _hash_string_error;

    if( pair->children[0].type == RPC_KEY && 
        pair->children[1].type == RPC_VALUE )
    {
      key = &pair->children[0];
      value = &pair->children[1];
    }
    else if( pair->children[0].type == RPC_VALUE && 
             pair->children[1].type == RPC_KEY )
    {
      key = &pair->children[1];
      value = &pair->children[0];
    }
    else
      goto _hash_string_error;

    if( key->numChildren != 1 || value->numChildren != 1 )
      goto _hash_string_error;

    if( _node_string(&key->children[0], &element_str, &element_len) == 0 )
      goto _hash_string_error;

    // XXX: Need a 64-bit realloc
    new_str = realloc(*str, (size_t)*len + element_len);

    if( !new_str )
      goto _hash_string_error2;

    *str = new_str;

    //XXX: Need a 64-bit memcpy

    memcpy(*str + *len, element_str, (size_t)element_len);
    *len += element_len;
    free(element_str);

    if( _node_string(&value->children[0], &element_str, &element_len) == 0 )
      goto _hash_string_error;

    // XXX: Need a 64-bit realloc
    new_str = realloc(*str, (size_t)*len + element_len);

    if( !new_str )
      goto _hash_string_error2;

    *str = new_str;

    //XXX: Need a 64-bit memcpy

    memcpy(*str + *len, element_str, (size_t)element_len);
    *len += element_len;
    free(element_str);
  }

  return 1;

_hash_string_error2:
  free(element_str);
_hash_string_error:
  free(*str);
  return 0;
}

static int _node_string(struct RPC_Node *node, uchar **str, uint64_t *len)
{
  switch( node->type )
  {
    case RPC_NULL:
      return _null_string(node,str,len);
    case RPC_TRUE:
      return _true_string(node,str,len);
    case RPC_FALSE:
      return _false_string(node,str,len);
    case RPC_BYTE:
      return _byte_string(node,str,len);
    case RPC_INT16:
      return _int16_string(node,str,len);
    case RPC_INT32:
      return _int32_string(node,str,len);
    case RPC_INT64:
      return _int64_string(node,str,len);
    case RPC_UUID:
      return _uuid_string(node,str,len);
    case RPC_FLOAT:
      return _float_string(node,str,len);
    default:
      break;
  }

  switch( node->type & 0xF0 )
  {
    case RPC_STRING:
      return _sequence_string(node,str,len);
    case RPC_ARRAY:
      return _array_string(node,str,len);
    case RPC_HASH:
      return _hash_string(node,str,len);
    default:
      break;
  }

  return 0;
}
