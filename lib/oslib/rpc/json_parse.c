#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "serialization.h"

typedef uint8_t uchar;

struct RPC_Node *parseJSON(uchar *s);
void showNodes(struct RPC_Node *node);

static void _showNodes(struct RPC_Node *node, int depth);
static void _skipWhitespace(uchar **s);
static int _parseTrue(uchar **s, struct RPC_Node *node);
static int _parseFalse(uchar **s, struct RPC_Node *node);
static int _parseNull(uchar **s, struct RPC_Node *node);
static int _parseString(uchar **s, struct RPC_Node *node);
static int _parseNumber(uchar **s, struct RPC_Node *node);
static int _parseArray(uchar **s, struct RPC_Node *node);
static int _parseObject(uchar **s, struct RPC_Node *node);
static int _parsePair(uchar **s, struct RPC_Node *node);
static int _parseValue(uchar **s, struct RPC_Node *node);

struct RPC_Node *parseJSON(uchar *s)
{
  struct RPC_Node *node = rpc_new_node();
  uchar *start = s;

  if( !node )
    return NULL;

  if( _parseValue(&s, node) == 0 )
  {
    printf("Parse error at char %d\n", s - start);
    return NULL;
  }
  else
    return node;
}

void showNodes(struct RPC_Node *node)
{
  _showNodes(node, 0);
}

static void _showNodes(struct RPC_Node *node, int depth)
{
  for( int i=depth*2; i; i-- )
    putchar(' ');

  switch( node->type )
  {
    case RPC_NULL:
      printf("RPCNull");
      break;
    case RPC_TRUE:
      printf("RPCTrue");
      break;
    case RPC_FALSE:
      printf("RPCFalse");
      break;
    case RPC_STRING:
      printf("RPCString (length: %llu) : \"%*s\"", node->data.string.len,
        (unsigned int)node->data.string.len, node->data.string.seq);
      break;
    case RPC_PAIR:
      printf("RPCPair");
      break;
    case RPC_KEY:
      printf("RPCKey");
      break;
    case RPC_VALUE:
      printf("RPCValue");
      break;
    case RPC_ARRAY:
      printf("RPCArray (size: %lld)", node->numChildren);
      break;
    case RPC_HASH:
      printf("RPCHash (size: %lld)", node->numChildren);
      break;
    case RPC_BYTE:
      printf("RPCByte : %d", node->data.b);
      break;
    case RPC_INT16:
      printf("RPCInt16 : %d", node->data.i16);
      break;
    case RPC_INT32:
      printf("RPCInt32 : %d", node->data.i32);
      break;
    case RPC_INT64:
      printf("RPCInt64 : %lld", node->data.i64);
      break;
    case RPC_FLOAT:
      printf("RPCFloat : %g", node->data.f);
      break;
    case RPC_UUID:
      printf("RPCUuid : %llx", *(long long unsigned int *)&node->data.uuid);
      break;
    default:
      printf("UnknownNode (type: %d)", node->type);
      break;
  }

  printf("\n");

  for(int i=0; i < node->numChildren; i++)
    _showNodes(&node->children[i], depth+1);
}

static void _skipWhitespace(uchar **s)
{
  while(**s == ' ')
    (*s)++;
}

static int _parseTrue(uchar **s, struct RPC_Node *node)
{
  _skipWhitespace(s);

  if( strncmp("true", (char *)*s, 4) == 0 )
  {
    *s += 4;
    return rpc_bool(true, node);
  }
  else
    return 0;
}

static int _parseFalse(uchar **s, struct RPC_Node *node)
{
  _skipWhitespace(s);

  if( strncmp("false", (char *)*s, 5) == 0 )
  {
    *s += 5;
    return rpc_bool(false, node);
  }
  else
    return 0;
}

static int _parseNull(uchar **s, struct RPC_Node *node)
{
  _skipWhitespace(s);

  if( strncmp("null", (char *)*s, 4) == 0 )
  {
    *s += 4;
    return rpc_null(node);
  }
  else
    return 0;
}

/* Assumes that the string is utf-8 */

static int _parseString(uchar **s, struct RPC_Node *node)
{
  uchar *start;
  int bytes_left=0;

  _skipWhitespace(s);

  if( **s == '\"' )
    start = ++(*s);
  else
    return 0;

  while( **s != '\"' && **s )
  {
    if( !bytes_left && **s == '\\' )
    {
      switch( *(++(*s)) )
      {
        case '\0':
          return 0;
        case '\"':
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
          ++(*s);
          continue;
        case 'u':
          ++(*s);

          for(int i=0; i < 4; i++, ++(*s))
          {
            if( !isxdigit(**s) )
              return 0;
          }

          continue;
        default:
          return 0;
      }
    }

    if( **s >= 0 && **s < 0x20 )
      return 0;
    else if( **s == 0xC0 || **s == 0xC1 || (**s >= 0xF5 && **s <= 0xFF) )
      return 0;
    else if( !bytes_left && **s >= 0x80 && **s <= 0xBF )
      return 0;
    else if( bytes_left && (**s <= 0x7F || **s >= 0xC2) )
      return 0;

    if( **s >= 0x80 && **s <= 0xBF )
      bytes_left--;

    if( **s >= 0xF0 )
      bytes_left = 3;
    else if( **s >= 0xE0 )
      bytes_left = 2;
    else if( **s >= 0xC2 )
      bytes_left = 1;

    (*s)++;
  }

  (*s)++;

  if( bytes_left || rpc_string(start, (size_t)(*s - start - 1), node) == 0 )
    return 0;
  else
    return 1;
}

static int _parseNumber(uchar **s, struct RPC_Node *node)
{
  bool neg = false, negExp = false, decimal = false;
  double number = 0;
  double multiplier=0.1;
  int exponent = 0;

  _skipWhitespace(s);

  if( **s == '-' )
  {
    neg = true;
    (*s)++;
  }

  if( isdigit(**s) )
  {
    if( **s != '0' )
    {
      number = **s - '0';
      (*s)++;

      while( isdigit(**s) )
      {
        number = number * 10 + (**s - '0');
        (*s)++;
      }
    }
  }
  else
    return 0;

  if( **s == '.' )
  {
    (*s)++;
    decimal = true;

    if( isdigit(**s) )
    {
      while( isdigit(**s) )
      {
        number += (**s - '0') * multiplier;
        multiplier /= 10;
        (*s)++;
      }
    }
    else
      return 0;
  }

  if( **s == 'e' || **s == 'E' )
  {
    (*s)++;
    decimal = true;

    if( **s == '+' )
      (*s)++;
    else if( **s == '-' )
    {
      (*s)++;
      negExp = true;
    }

    if( isdigit(**s) )
    {
      exponent = (**s - '0') + exponent * 10;
      (*s)++;
    }
    else
      return 0;
  }

  if( negExp )
    exponent = -exponent;

  if( exponent != 0 )
    number = pow(number, exponent);

  if( neg )
    number = -number;

  if( decimal )
  {
    node->type = RPC_FLOAT;
    node->data.f = number;
  }
  else if( number < 0 )
  {
    if( number < LLONG_MIN )
    {
      node->type = RPC_FLOAT;
      node->data.f = number;
    }
    else if( number < INT_MIN )
    {
      node->type = RPC_INT64;
      node->data.i64 = (int64_t)number;
    }
    else if( number < SHRT_MIN )
    {
      node->type = RPC_INT32;
      node->data.i32 = (int32_t)number;
    }
    else if( number < SCHAR_MIN )
    {
      node->type = RPC_INT16;
      node->data.i16 = (int16_t)number;
    }
    else
    {
      node->type = RPC_CHAR;
      node->data.b = (int8_t)number;
    }
  }
  else
  {
    if( number > LLONG_MAX )
    {
      node->type = RPC_FLOAT;
      node->data.f = number;
    }
    else if( number > INT_MAX )
    {
      node->type = RPC_INT64;
      node->data.i64 = (int64_t)number;
    }
    else if( number > SHRT_MAX )
    {
      node->type = RPC_INT32;
      node->data.i32 = (int32_t)number;
    }
    else if( number > SCHAR_MAX )
    {
      node->type = RPC_INT16;
      node->data.i16 = (int16_t)number;
    }
    else
    {
      node->type = RPC_CHAR;
      node->data.b = (int8_t)number;
    }
  }

  return 1;
}

static int _parseArray(uchar **s, struct RPC_Node *node)
{
  struct RPC_Node *newNode = NULL;
  _skipWhitespace(s);

  rpc_array(node);

  if( **s == '[' )
  {
    (*s)++;
    _skipWhitespace(s);
  }
  else
    return 0;

  while( **s && **s != ']' )
  {
    newNode = rpc_new_node();

    if( _parseValue(s, newNode) == 0 || rpc_append_child(newNode, node) == 0 )
      goto _parseArrayError;

    newNode = NULL;
    _skipWhitespace(s);

    if( **s != ',' && **s != ']' )
      goto _parseArrayError;
    else if( **s == ',' )
    {
      (*s)++;
      _skipWhitespace(s);
    }
  }

  if( **s != ']' )
    goto _parseArrayError;

  (*s)++;
  return 1;

_parseArrayError:
  rpc_delete_node(newNode);
  rpc_remove_children(node);
  return 0;
}

static int _parseObject(uchar **s, struct RPC_Node *node)
{
  struct RPC_Node *newNode = NULL;
  _skipWhitespace(s);

  rpc_hash(node);

  if( **s == '{' )
  {
    (*s)++;
    _skipWhitespace(s);
  }
  else
    return 0;

  while( **s && **s != '}' )
  {
    newNode = rpc_new_node();

    if( _parsePair(s, newNode) == 0 || rpc_append_child(newNode, node) == 0 )
      goto _parseObjectError;

    newNode = NULL;
    _skipWhitespace(s);

    if( **s != ',' && **s != '}' )
      goto _parseObjectError;
    else if( **s == ',' )
    {
      (*s)++;
      _skipWhitespace(s);
    }
  }

  if( **s != '}' )
    goto _parseObjectError;

  (*s)++;

  return 1;

_parseObjectError:
  rpc_delete_node(newNode);
  rpc_remove_children(node);
  return 0;
}

static int _parsePair(uchar **s, struct RPC_Node *node)
{
  struct RPC_Node *newNodes[4] = { rpc_new_node(), rpc_new_node(),
    rpc_new_node(), rpc_new_node() };

  node->type = RPC_PAIR;

  if( !newNodes[0] || !newNodes[1] || !newNodes[2] || !newNodes[3] )
    goto _parsePairError;

  _skipWhitespace(s);

  newNodes[2]->type = RPC_KEY;
  newNodes[3]->type = RPC_VALUE;

  if( _parseString(s, newNodes[0]) == 0 ||
      rpc_append_child(newNodes[0], newNodes[2]) == 0 )
  {
    goto _parsePairError;
  }

  newNodes[0] = NULL;

  _skipWhitespace(s);

  if( **s != ':' )
    goto _parsePairError;

  (*s)++;

  _skipWhitespace(s);

  if( _parseValue(s, newNodes[1]) == 0 ||
      rpc_append_child(newNodes[1], newNodes[3]) == 0 )
  {
    goto _parsePairError;
  }

  newNodes[1] = NULL;

  if( rpc_append_child(newNodes[2], node) == 0 )
  {
    goto _parsePairError;
  }

  newNodes[2] = NULL;

  if( rpc_append_child(newNodes[3], node) == 0 )
    goto _parsePairError;

  return 1;

_parsePairError:
  rpc_delete_node(newNodes[0]);
  rpc_delete_node(newNodes[1]);
  rpc_delete_node(newNodes[2]);
  rpc_delete_node(newNodes[3]);
  rpc_remove_children(node);
  return 0;
}

static int _parseValue(uchar **s, struct RPC_Node *node)
{
  uchar *ptr = *s;
  int (*parseFuncs[7])(uchar **, struct RPC_Node *) = { _parseNumber,
    _parseString, _parseTrue, _parseFalse, _parseNull, _parseArray,
    _parseObject };

  for(int i=0; i < 7; i++)
  {
    *s = ptr;

    if( parseFuncs[i](s,node) == 1 )
      return 1;
  }

  return 0;
}
