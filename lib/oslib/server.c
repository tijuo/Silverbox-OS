#include <oslib.h>
#include <os/server.h>
#include <os/message.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/*
void server_loop(PCT *pct)
{
  multi_server_loop(1, pct);
}
*/
struct Message _msg_;

void server_loop( void )
{
  int _error;

  while(1)
  {
 //   __wait_for_msg(NULL_MID); // XXX: This requires a timer to work properly

    for(int i=0; i < _num_pcts; i++)
    {
      __test_for_msg(pctTable[i].mid, true); // doesn't work well for more than one pct

      while( !__MQUEUE_EMPTY(pctTable[i].queue) )
      {
        if( (_error = __get_message( pctTable[i].queue, &_msg_ )) != 0 )
        {
          print("Error! "), /*printInt(_error),*/ print("\n");
          continue;
        }

        if( pctTable[i].handlers[_msg_.protocol] != NULL )
          (pctTable[i].handlers[_msg_.protocol])(&_msg_);
      }
    }
  }
}

void _init_pct_table(void)
{
  _num_pcts = 0;

  for(int i=0; i < MAX_PCTS; i++)
    pctTable[i].mid = NULL_MID;
}

int attachPCT( PCT *pct )
{
  if( _num_pcts >= MAX_PCTS )
    return -1;

  pctTable[_num_pcts++] = *pct;
  return 0;
}

int detachPCT( mid_t mbox )
{
  if( _num_pcts == 0 )
    return -1;

  for(int i=0; i < _num_pcts; i++)
  {
    if( pctTable[i].mid == mbox )
    {
      for( int j=i+1; j < _num_pcts; j++ )
        pctTable[j-1] = pctTable[j];

      pctTable[--_num_pcts].mid = NULL_MID;
      return 0;
    }
  }

  return -1;
}

int __init_pct( PCT *pct, mid_t mid, struct MessageQueue *queue )
{
  if( pct == NULL || queue == NULL || mid == NULL_MID)
    return -1;

  pct->mid = mid;
  pct->queue = queue;
  memset(pct->handlers, 0, sizeof pct->handlers);
  return 0;
}

PCT *__create_pct( mid_t mid, struct MessageQueue *queue )
{
  PCT *new_pct;

  if( queue == NULL || mid == NULL_MID )
    return NULL;

  new_pct = malloc( sizeof(PCT) );

  if( new_pct == NULL )
    return NULL;

  if( __init_pct(new_pct, mid, queue) < 0 )
  {
    free( new_pct );
    return NULL;
  }
  else
    return new_pct;
}

int __set_protocol_handler( PCT *pct, int hnum, void (*fptr)(struct Message *) )
{
  if( pct == NULL || hnum >= MAX_PROTOCOLS || hnum < 0 )
    return -1;

  pct->handlers[hnum] = fptr;

  return 0;
}

int __get_protocol_handler( PCT *pct, int hnum, void (**handler)(struct Message *) )
{
  if( pct == NULL || hnum >= MAX_PROTOCOLS || hnum < 0 )
    return -1;
  else if(handler == NULL )
    return -1;
  
  *handler = pct->handlers[hnum];

  return 0;
}
