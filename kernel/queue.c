#include <kernel/queue.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>

TCB *timerDetach( TCB *thread );
TCB *timerEnqueue( TCB *thread, unsigned int time );
TCB *timerPop( void );
TCB *enqueue( struct Queue *queue, TCB *thread );
TCB *detachQueue( struct Queue *queue, TCB *thread );
TCB *popQueue( struct Queue *queue );
bool isInQueue( const struct Queue *queue, const TCB *thread );
bool isInTimerQueue( const TCB *thread );

#if DEBUG
unsigned int totalTimerTime(void);
#endif

bool isInQueue( const struct Queue *queue, const TCB *thread )
{
  const TCB *ptr;

  if( !queue || !thread )
    return false;

  for( ptr=queue->head; ptr != NULL && ptr != thread; ptr = ptr->queueNext );

  if( ptr == thread )
    return true;
  else
    return false;
}

bool isInTimerQueue( const TCB *thread )
{
  return isInQueue( &timerQueue, thread );
}

/**
  Adds a thread to the timer queue with an associated time.

  @param thread The thread to enqueue.
  @param time The delta time in ticks. Must be non-zero.
  @return The enqueued thread on success. NULL on failure.
*/

TCB *timerEnqueue( TCB *thread, unsigned int time )
{
  TCB *prev;
  TCB *ptr;

  if( time == 0 || !thread )
    RET_MSG(NULL, "Invalid time or NULL pointer")

  assert( !isInTimerQueue( thread ) );

  #if DEBUG
    unsigned int level;

    for( level=0; level < NUM_RUN_QUEUES; level++ )
      assert( !isInQueue( &runQueues[level], thread ) );
  #endif

  if( timerQueue.head == NULL )
  {
    timerQueue.head = timerQueue.tail = thread;
    thread->timerDelta = time;
    thread->timerNext = NULL;

    return thread;
  }

  for( prev=NULL, ptr=timerQueue.head; ptr != NULL; prev=ptr, ptr=ptr->timerNext )
  {
    if( time > ptr->timerDelta )
    {
      time -= ptr->timerDelta;

      if( ptr->timerNext == NULL )
      {
        thread->timerDelta = time;
        thread->timerNext = NULL;
        ptr->timerNext = timerQueue.tail = thread;
        break;
      }
      else
        continue;
    }
    else
    {
      if( prev == NULL )
        timerQueue.head = thread;
      else
        prev->timerNext = thread;

      thread->timerNext = ptr;
      thread->timerDelta = time;
      ptr->timerDelta -= time;

      break;
    }
  }

  return thread;
}

/**
  Removes the first thread from the timer queue if that thread has no time left.

  @return The dequeued thread. NULL on failure.
*/

TCB *timerPop( void )
{
  return timerDetach(timerQueue.head);
}

#if DEBUG

unsigned int totalTimerTime(void)
{
  unsigned int total = 0;
  const TCB *ptr;

  for(ptr=timerQueue.head; ptr != NULL; ptr = ptr->timerNext)
    total += ptr->timerDelta;

  return total;
}


#endif

/**
  Removes a thread from the timer queue.

  @param thread The thread to detach.
  @return The detached thread if successful. NULL if unsuccessful.
*/

TCB *timerDetach( TCB *thread )
{
  if( !thread )
    RET_MSG(NULL, "NULL thread");

  #if DEBUG
    unsigned int total_bef=totalTimerTime();
  #endif

  if( thread == timerQueue.head )
  {
    timerQueue.head = thread->timerNext;

    if( thread != timerQueue.tail )
    {
      assert( thread->timerNext != NULL );
      thread->timerNext->timerDelta += thread->timerDelta;
    }
    else
    {
      timerQueue.tail = NULL;
    }

    #if DEBUG
      if( thread->timerNext == NULL )
        assert( totalTimerTime() + thread->timerDelta == total_bef );
      else
        assert( totalTimerTime() == total_bef );
    #endif
  }
  else
  {
    TCB *ptr;

    for( ptr=timerQueue.head; ptr != NULL; ptr = ptr->timerNext )
    {
      if( ptr->timerNext == thread )
      {
        ptr->timerNext = thread->timerNext;
        assert( thread->timerNext != NULL );
        thread->timerNext->timerDelta += thread->timerDelta;

        #if DEBUG
          if( thread->timerNext == NULL )
            assert( totalTimerTime() + thread->timerDelta == total_bef );
          else
            assert( totalTimerTime() == total_bef );
        #endif

        if( thread == timerQueue.tail )
          timerQueue.tail = ptr;
      }
    }

    if( ptr == NULL )
      return NULL;
  }

  thread->timerNext = NULL;
  thread->timerDelta = 0;

  return thread;
}

/**
  Adds a thread to the end of a thread queue.

  @param queue The queue to which a thread will be attached.
  @param thread The thread to enqueue.
  @return The enqueued thread on success. NULL on failure.
*/

TCB *enqueue( struct Queue *queue, TCB *thread )
{
  if( !queue || !thread )
    RET_MSG(NULL, "NULL pointer")//return -1;

  assert( thread->threadState != RUNNING );

  assert( (queue->head == NULL && queue->tail == NULL) ||
          (queue->head == queue->tail &&
             queue->head->queuePrev == queue->head->queueNext
             && queue->head->queueNext == NULL ) ||
          (queue->head != NULL && queue->tail != NULL) );

  assert( !isInQueue( queue, thread ) );
  assert( !isInTimerQueue( thread ) || (thread->waitThread != NULL &&
    (queue == &thread->waitThread->threadQueue)) );

  for( unsigned int level=0; level < NUM_RUN_QUEUES; level++ )
    assert( !isInQueue( &runQueues[level], thread ) );

  thread->queuePrev = queue->tail;
  thread->queueNext = NULL;

  if( queue->tail )
    queue->tail->queueNext = thread;

  queue->tail = thread;

  if( !queue->head )
    queue->head = queue->tail;

  assert( isInQueue( queue, thread ) );

  return thread;
}

/**
  Removes a thread from a thread queue.

  @param queue The thread queue from which to detach.
  @param thread The thread to detach.
  @return The detached thread, if found. NULL if not found or failure.
*/

TCB *detachQueue( struct Queue *queue, TCB *thread )
{
  assert( queue );
  assert( (queue->head == NULL && queue->tail == NULL) ||
          (queue->head == queue->tail &&
             queue->head->queuePrev == queue->head->queueNext
             && queue->head->queueNext == NULL ) ||
          (queue->head != NULL && queue->tail != NULL) );

  if( !queue || !queue->head || !thread )
    return NULL;

  assert( thread->threadState != RUNNING );

  assert( !isInTimerQueue( thread ) || (thread->waitThread != NULL &&
          (queue == &thread->waitThread->threadQueue)) );

  for( TCB *ptr=queue->head; ptr; ptr=ptr->queueNext )
  {
    if( ptr == thread )
    {
      if( ptr->queuePrev )
        ptr->queuePrev->queueNext = ptr->queueNext;
      if( ptr->queueNext )
        ptr->queueNext->queuePrev = ptr->queuePrev;

      if( queue->head == thread )
        queue->head = ptr->queueNext;
      if( queue->tail == thread )
        queue->tail = ptr->queuePrev;

      ptr->queueNext = ptr->queuePrev = NULL;
      return ptr;
    }
  }

  return NULL;
}

/**
  Removes the first thread from a thread queue.

  @param queue A thread queue.
  @return The first thread on the thread queue. NULL if the
          thread queue is empty or on failure.
*/

TCB *popQueue( struct Queue *queue )
{
  return detachQueue( queue, queue->head );
}
