#include <kernel/queue.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>

int timerDetach( tid_t tid );
int timerEnqueue( tid_t tid, unsigned short time );
tid_t timerPop( void );
int enqueue( struct Queue *queue, tid_t tid );
tid_t detachQueue( struct Queue *queue, tid_t tid );
tid_t popQueue( struct Queue *queue );
bool isInQueue( struct Queue *queue, tid_t tid );
bool isInTimerQueue( tid_t tid );

#if DEBUG
int totalTimerTime(void);
#endif

bool isInQueue( struct Queue *queue, tid_t tid )
{
  if( !queue || tid == NULL_TID )
    return false;

  for( tid_t ptr=queue->head; ptr != NULL_TID; ptr = tcbNodes[ptr].next )
  {
    if( tid == ptr )
      return true;
  }

  return false;
}

bool isInTimerQueue( tid_t tid )
{
  if( tid == NULL_TID )
    return false;

  for( tid_t ptr=timerQueue.head; ptr != NULL_TID; ptr = timerNodes[ptr].next )
  {
    if( tid == ptr )
      return true;
  }

  return false;
}

/**
  Adds a thread to the timer queue with an associated time.

  @param tid The TID of the thread to enqueue.
  @param time The delta time in ticks. Must be non-zero.
  @return 0 on success. -1 on failure.
*/

int timerEnqueue( tid_t tid, unsigned short time )
{
  tid_t node, prevNode = NULL_TID;
  struct TimerNode ptr;

  if( time < 1 || tid == NULL_TID )
    RET_MSG(-1, "Invalid time or NULL pointer")

  assert( !isInTimerQueue( tid ) );

  #if DEBUG
    for( int level=0; level < maxRunQueues; level++ )
      assert( !isInQueue( &runQueues[level], tid ) );
  #endif

  if( timerQueue.head == NULL_TID )
  {
    timerQueue.head = tid; // No need for using a tail
    timerNodes[tid].delta = time;
    timerNodes[tid].next = NULL_TID;

    return 0;
  }

  for( node = timerQueue.head; node != NULL_TID; prevNode=node,node = ptr.next )
  {
    ptr = timerNodes[node];

    if( time > ptr.delta )
    {
      time -= ptr.delta;

      if( ptr.next == NULL_TID )
      {
        timerNodes[tid].delta = time;
        timerNodes[tid].next = NULL_TID;
        timerNodes[node].next = tid;

        break;
      }
      else
        continue;
    }
    else
    {
      if( prevNode == NULL_TID )
        timerQueue.head = tid;
      else
        timerNodes[prevNode].next = tid;


      timerNodes[tid].next = node;
      timerNodes[tid].delta = time;
      timerNodes[node].delta -= time;

      break;
    }
  }

  return 0;
}

/**
  Removes the first thread from the timer queue if that thread has no time left.

  @return The TID of the dequeued thread. NULL_TID on failure.
*/

tid_t timerPop( void )
{
  tid_t node = timerQueue.head;

  if( timerDetach( node ) == 0 )
    return node;
  else
    return NULL_TID;
}

#if DEBUG

int totalTimerTime(void)
{
  int total = 0;

  for(tid_t ptr=timerQueue.head; ptr != NULL_TID; ptr = timerNodes[ptr].next)
    total += timerNodes[ptr].delta;

  return total;
}


#endif

/**
  Removes a thread from the timer queue.

  @param tid The TID of the thread to detach.
  @return 0 on success. -1 on failure.
*/

int timerDetach( tid_t tid )
{
  if( tid == NULL_TID )
    RET_MSG(-1, "NULL ptr/tid");

  #if DEBUG
//    for( int level=0; level < maxRunQueues; level++ )
//      assert( !isInQueue( &runQueues[level], tid ) );

    int total_bef=totalTimerTime();
  #endif

  if( tid == timerQueue.head )
  {
    if( timerNodes[tid].next != NULL_TID )
      timerNodes[timerNodes[tid].next].delta += timerNodes[tid].delta;

    timerQueue.head = timerNodes[tid].next;

    #if DEBUG
      if( timerNodes[tid].next == NULL_TID )
        assert( totalTimerTime() + timerNodes[tid].delta == total_bef );
      else
        assert( totalTimerTime() == total_bef );
    #endif

    timerNodes[tid].delta = 0;
    timerNodes[tid].next = NULL_TID;

    return 0;
  }

  for( tid_t ptr=timerQueue.head; ptr != NULL_TID; ptr = timerNodes[ptr].next )
  {
    if( timerNodes[ptr].next == tid )
    {
      timerNodes[ptr].next = timerNodes[tid].next;
      timerNodes[timerNodes[tid].next].delta += timerNodes[tid].delta;

      #if DEBUG
        if( timerNodes[tid].next == NULL_TID )
          assert( totalTimerTime() + timerNodes[tid].delta == total_bef );
        else
          assert( totalTimerTime() == total_bef );
      #endif

      timerNodes[tid].delta = 0;
      timerNodes[tid].next = NULL_TID;

      return 0;
    }
  }

  return -1;
}

/**
  Adds a thread to the end of a thread queue.

  @param tid The TID of the thread to enqueue.
  @return 0 on success. -1 on failure.
*/

int enqueue( struct Queue *queue, tid_t tid )
{
  if( queue == NULL || tid == NULL_TID )
    RET_MSG(-1, "NULL ptr/tid")//return -1;

  assert( (queue->head == NULL_TID && queue->tail == NULL_TID)
          || (queue->head != NULL_TID && queue->tail != NULL_TID) );

  assert( !isInQueue( queue, tid ) );
  assert( !isInTimerQueue( tid ) || (tcbTable[tid].wait_tid != NULL_TID && (queue == &tcbTable[tcbTable[tid].wait_tid].threadQueue)) );

  for( int level=0; level < maxRunQueues; level++ )
    assert( !isInQueue( &runQueues[level], tid ) );

  tcbNodes[tid].prev = queue->tail;
  tcbNodes[tid].next = NULL_TID;

  if( queue->tail != NULL_TID )
    tcbNodes[queue->tail].next = tid;

  queue->tail = tid;

  if( queue->head == NULL_TID )
    queue->head = queue->tail;

  assert( isInQueue( queue, tid ) );

  assert( queue->head != NULL_TID && queue->tail != NULL_TID );

  return 0;
}

/**
  Removes a thread from a thread queue.

  @param queue A thread queue.
  @param tid The TID of the thread to dequeue.
  @return The TID of the thread, if found. NULL_TID if the thread is not on the queue or on failure.
*/

tid_t detachQueue( struct Queue *queue, tid_t tid )
{
  tid_t nodeTID;
  struct NodePointer *ptr;

  assert( (queue->head != NULL_TID && queue->tail != NULL_TID) ||
          (queue->head == NULL_TID && queue->tail == NULL_TID) );

  if( queue == NULL || queue->head == NULL_TID || tid == NULL_TID )
    return NULL_TID;

  assert( !isInTimerQueue( tid ) || (tcbTable[tid].wait_tid != NULL_TID && (queue == &tcbTable[tcbTable[tid].wait_tid].threadQueue)) );

/*
  for( int level=0; level < maxRunQueues; level++ )
  {
    if( queue == &runQueues[level] )
      continue;

    assert( !isInQueue( &runQueues[level], tid ) );
  }
*/

  for( nodeTID = queue->head; nodeTID != NULL_TID; nodeTID = ptr->next )
  {
    ptr = &tcbNodes[nodeTID];

    if( nodeTID == tid )
    {
      if( ptr->prev != NULL_TID )
        tcbNodes[ptr->prev].next = ptr->next;
      if( ptr->next != NULL_TID )
        tcbNodes[ptr->next].prev = ptr->prev;

      if( queue->head == nodeTID )
        queue->head = ptr->next;
      if( queue->tail == nodeTID )
        queue->tail = ptr->prev;

      tcbNodes[nodeTID].next = tcbNodes[nodeTID].prev = NULL_TID;

      return nodeTID;
    }
  }

return NULL_TID;//  RET_MSG(NULL_TID, "NULL tid")//return NULL_TID;
}

/**
  Removes the first thread from a thread queue.

  @param queue A thread queue.
  @return The TID of the first thread on the thread queue. NULL_TID if the
          thread queue is empty or on failure.
*/

tid_t popQueue( struct Queue *queue )
{
  if( queue == NULL )
    return NULL_TID;

  return detachQueue( queue, queue->head );
}
