#include <kernel/queue.h>
#include <kernel/debug.h>
#include <kernel/thread.h>

/* Done for now. */

int sleepEnqueue( struct Queue *queue, tid_t tid, unsigned short time );
tid_t sleepDequeue( struct Queue *queue );
int enqueue( struct Queue *queue, tid_t tid );
tid_t detachQueue( struct Queue *queue, tid_t tid );
tid_t dequeue( struct Queue *queue );

/* !!! This function may cause problems. Must test !!! */

int sleepEnqueue( struct Queue *queue, tid_t tid, unsigned short time )
{
  tid_t node, prevNode = NULL_TID;
  struct NodePointer ptr;

  assert( queue != NULL );
  assert( tid != NULL_TID );
  assert( time > 0 );

  if( time < 1 || queue == NULL || tid == NULL_TID )
    RET_MSG(-1, "Invalid time or NULL pointer")//return -1;

  if( queue->head == NULL_TID )
  {
    queue->head = tid; // No need for using a tail
    tcbNodes[tid].delta = time;
    tcbNodes[tid].next = NULL_TID;

    return 0;
  }

  for( node = queue->head; node != NULL_TID; prevNode=node,node = ptr.next )
  {
    ptr = tcbNodes[node];

    if( time > ptr.delta )
    {
      time -= ptr.delta;

      if( ptr.next == NULL_TID )
      {
        tcbNodes[tid].delta = time;
        tcbNodes[tid].next = NULL_TID;
        tcbNodes[node].next = tid;

        break;
      }
      else
        continue;
    }
    else
    {
      if( prevNode == NULL_TID )
        queue->head = tid;
      else
        tcbNodes[prevNode].next = tid;


      tcbNodes[tid].next = node;
      tcbNodes[tid].delta = time;
      tcbNodes[node].delta -= time;

      break;
    }
  }

  return 0;
}

/* Dequeues a thread only if the head has 0 delta. */

tid_t sleepDequeue( struct Queue *queue )
{
  tid_t node = NULL_TID;

  assert( queue != NULL );

  if( queue == NULL || queue->head == NULL_TID )
    return NULL_TID;

  if( tcbNodes[queue->head].delta == 0 )
  {
    node = queue->head;
    queue->head = tcbNodes[queue->head].next;
  }

  return node;
}

int enqueue( struct Queue *queue, tid_t tid )
{
  assert( queue != NULL );
  assert( tid != NULL_TID );

  if( queue == NULL || tid == NULL_TID )
    RET_MSG(-1, "NULL ptr/tid")//return -1;

  tcbNodes[tid].prev = queue->tail;
  tcbNodes[tid].next = NULL_TID;

  if( queue->tail != NULL_TID )
    tcbNodes[queue->tail].next = tid;

  queue->tail = tid;

  if( queue->head == NULL_TID )
    queue->head = queue->tail;

  return 0;
}

tid_t detachQueue( struct Queue *queue, tid_t tid )
{
  tid_t nodeTID;
  struct NodePointer *ptr;

  assert( queue != NULL );
//  assert( tid != NULL_TID );

  if( queue == NULL || queue->head == NULL_TID || tid == NULL_TID )
    return NULL_TID;

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

tid_t dequeue( struct Queue *queue )
{
  assert( queue != NULL );

  if( queue == NULL )
    return NULL_TID;//RET_MSG(NULL_TID, "NULL queue")//return NULL_TID;

  return detachQueue( queue, queue->head );
}
