#include <kernel/queue.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/schedule.h>

tcb_t *timerDetach( tcb_t *thread );
tcb_t *timerEnqueue( tcb_t *thread, unsigned int time );
tcb_t *timerPop( void );
tcb_t *enqueue( struct Queue *queue, tcb_t *thread );
tcb_t *detachQueue( struct Queue *queue, tcb_t *thread );
tcb_t *popQueue( struct Queue *queue );
int isInQueue( const struct Queue *queue, const tcb_t *thread );
int isInTimerQueue( const tcb_t *thread );

#if DEBUG
unsigned int totalTimerTime(void);
#endif

int isInQueue(const struct Queue *queue, const tcb_t *thread)
{
  const tcb_t *ptr;

  for(ptr=queue->head; ptr != thread && ptr != NULL; ptr=getTcb(ptr->queue.next));

  return (ptr == thread);
}

int isInTimerQueue( const tcb_t *thread )
{
  return isInQueue( &timerQueue, thread );
}

/**
  Adds a thread to the timer queue with an associated time.

  @param thread The thread to enqueue.
  @param time The delta time in ticks. Must be non-zero.
  @return The enqueued thread on success. NULL on failure.
*/

tcb_t *timerEnqueue( tcb_t *thread, unsigned int time )
{
  tcb_t *prev, *ptr;

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
    thread->queue.delta = time;
    thread->queue.next = NULL_TID;

    return thread;
  }

  for( prev=NULL, ptr=timerQueue.head; ptr != NULL; prev=ptr, ptr=getTcb(ptr->queue.next) )
  {
    if( time > ptr->queue.delta )
    {
      time -= ptr->queue.delta;

      if( ptr->queue.next == NULL_TID )
      {
        thread->queue.delta = time;
        thread->queue.next = NULL_TID;
        ptr->queue.next = getTid(thread);
        timerQueue.tail = thread;
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
        prev->queue.next = getTid(thread);

      thread->queue.next = getTid(ptr);
      thread->queue.delta = time;
      ptr->queue.delta -= time;

      break;
    }
  }

  return thread;
}

/**
  Removes the first thread from the timer queue if that thread has no time left.

  @return The dequeued thread. NULL on failure.
*/

tcb_t *timerPop( void )
{
  return timerDetach(timerQueue.head);
}

#if DEBUG

unsigned int totalTimerTime(void)
{
  unsigned int total=0;

  for(const tcb_t *ptr=timerQueue.head; ptr != NULL; ptr=getTcb(ptr->queue.next))
    total += ptr->queue.delta;

  return total;
}


#endif

/**
  Removes a thread from the timer queue.

  @param thread The thread to detach.
  @return The detached thread if successful. NULL if unsuccessful.
*/

tcb_t *timerDetach( tcb_t *thread )
{
  if( !thread )
    RET_MSG(NULL, "NULL thread");

  #if DEBUG
    unsigned int total_bef=totalTimerTime();
  #endif

  if( thread == timerQueue.head )
  {
    timerQueue.head = getTcb(thread->queue.next);

    if( thread != timerQueue.tail )
    {
      assert( thread->queue.next != NULL_TID );
      getTcb(thread->queue.next)->queue.delta += thread->queue.delta;
    }
    else
      timerQueue.tail = NULL;

    #if DEBUG
      if( thread->queue.next == NULL_TID )
        assert( totalTimerTime() + thread->queue.delta == total_bef );
      else
        assert( totalTimerTime() == total_bef );
    #endif
  }
  else
  {
    tcb_t *ptr;

    for(ptr=timerQueue.head; ptr != NULL; ptr=getTcb(ptr->queue.next))
    {
      if(ptr->queue.next == getTid(thread))
      {
        ptr->queue.next = thread->queue.next;
        assert( thread->queue.next != NULL_TID );
        getTcb(thread->queue.next)->queue.delta += thread->queue.delta;

        #if DEBUG
          if( thread->queue.next == NULL_TID )
            assert( totalTimerTime() + thread->queue.delta == total_bef );
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

  thread->queue.next = NULL_TID;
  thread->queue.delta = 0;

  return thread;
}

/**
  Adds a thread to the end of a thread queue.

  @param queue The queue to which a thread will be attached.
  @param thread The thread to enqueue.
  @return The enqueued thread on success. NULL on failure.
*/

tcb_t *enqueue( struct Queue *queue, tcb_t *thread )
{
  if( !queue || !thread )
    RET_MSG(NULL, "NULL pointer")//return -1;

  assert( thread->threadState != RUNNING );

  assert( (queue->head == NULL && queue->tail == NULL) ||
          (queue->head == queue->tail &&
             queue->head->queue.prev == queue->head->queue.next
             && queue->head->queue.next == NULL_TID ) ||
          (queue->head != NULL && queue->tail != NULL) );

  assert( !isInQueue( queue, thread ) );
  assert( !isInTimerQueue( thread ) );

  for( unsigned int level=0; level < NUM_RUN_QUEUES; level++ )
    assert( !isInQueue( &runQueues[level], thread ) );

  thread->queue.prev = getTid(queue->tail);
  thread->queue.next = NULL_TID;

  if( queue->tail )
    queue->tail->queue.next = getTid(thread);

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

tcb_t *detachQueue( struct Queue *queue, tcb_t *thread )
{
  assert( queue );
  assert( (queue->head == NULL && queue->tail == NULL) ||
          (queue->head == queue->tail &&
             queue->head->queue.prev == queue->head->queue.next
             && queue->head->queue.next == NULL_TID ) ||
          (queue->head != NULL && queue->tail != NULL) );

  if( !queue || !queue->head || !thread )
    return NULL;

  assert( thread->threadState != RUNNING );

  assert( !isInTimerQueue( thread ) );

  for( tcb_t *ptr=queue->head; ptr; ptr=getTcb(ptr->queue.next) )
  {
    if( ptr == thread )
    {
      if( ptr->queue.prev )
        getTcb(ptr->queue.prev)->queue.next = ptr->queue.next;
      if( ptr->queue.next )
        getTcb(ptr->queue.next)->queue.prev = ptr->queue.prev;

      if( queue->head == thread )
        queue->head = getTcb(ptr->queue.next);
      if( queue->tail == thread )
        queue->tail = getTcb(ptr->queue.prev);

      ptr->queue.next = ptr->queue.prev = NULL_TID;
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

tcb_t *popQueue( struct Queue *queue )
{
  return detachQueue( queue, queue->head );
}
