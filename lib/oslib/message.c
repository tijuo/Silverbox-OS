#include <string.h>
#include <oslib.h>
#include <os/message.h>
#include <os/mutex.h>
#include <os/server.h>

int __post_message( struct MessageQueue *queue, struct Message *msg );
int __get_message( struct MessageQueue *queue, struct Message *msg );
int _get_message( struct MessageQueue *queue, tid_t *sender, size_t *msg_len,
                  mid_t *recipient, mid_t *reply, void *buffer,
                  unsigned *protocol, size_t buffer_len );
int _post_message( struct MessageQueue *queue, unsigned protocol,
                  mid_t recipient, mid_t reply, void *data, size_t len );

/* User posts messages at the tail while kernel removes messages from the head (while sending).
   User gets messages from the head(while receiving) while kernel adds them onto the tail.
   As long as a message queue is not both an output and input queue. Everything should
   work without synchronization issues. It's very important that the user only increment
   the head(ptr) if there is a valid message in the new position and increment the
   tail only after retrieving the new message. Not doing this will result in bogus
   messages being sent or a message to be overwritten while retrieving it.*/

/* XXX: Do the locks actually work? */

int __post_message( struct MessageQueue *queue, struct Message *msg )
{
  if( queue == NULL || msg == NULL || queue->pointer == NULL || 
      queue->tail == NULL )
  {
    return -1;
  }
//  else if( mutex_lock(&queue->lock) )
//    return -2;

  while( mutex_lock(&queue->lock) )
    __yield();

  if( ( *queue->tail + 1 ) % queue->length == *queue->pointer )
  {
    mutex_unlock(&queue->lock);
    return 1;
  }

  queue->queueAddr[*queue->tail] = *msg;
  *queue->tail = (*queue->tail + 1) % queue->length;
  mutex_unlock(&queue->lock);

  return 0;
}

int _post_message( struct MessageQueue *queue, unsigned protocol, 
                  mid_t recipient, mid_t reply, void *data, size_t len )
{
  volatile struct Message *msg;

  if( len > MAX_MSG_LEN )
  {
    print("Too long!\n");
    return -1;
  }
  else if( queue == NULL || queue->pointer == NULL || queue->tail == NULL || 
           recipient == NULL_MID )
  {
    return -1;
  }
//  else if( mutex_lock(&queue->lock) )
//    return -2;

  while( mutex_lock(&queue->lock) )
    __yield();

  if( ( *queue->tail + 1 ) % queue->length == *queue->pointer )
  {
    mutex_unlock(&queue->lock);
    return 1;
  }

  msg = &queue->queueAddr[*queue->tail];


  msg->recipient = recipient;
/*
  if( _num_pcts != 1 )
  {
    print("Wrong3: "), printHex(msg), print(" "), printHex(&msg->recipient), print("\n");
  }
*/
  msg->length = (len > MAX_MSG_LEN ? MAX_MSG_LEN : len);
/*
  if( _num_pcts != 1 )
  {
    print("Wrong4: 0x"), printHex(&queue->queueAddr[*queue->tail]), 
print(" 0x"), printHex(*queue->tail), print(" "), printHex(*queue->pointer), print(" "),
print(" 0x"), printHex(queue->tail), print(" "), printHex(queue->pointer), print(" ");
print(" 0x"), printHex(queue->queueAddr), print("\n");
  }
*/
  msg->protocol = protocol;
/*
  if( _num_pcts != 1 )
  {
    print("Wrong5: "), printHex(msg), print("\n");
  }
*/
  msg->reply_mid = reply;
/*
  if( _num_pcts != 1 )
  {
    print("Wrong6: "), printHex(msg), print(" "), printHex(&msg->reply_mid), print("\n");
  }
*/

  /* XXX: Return an error code if the data can't fit in a message */

  if( data != NULL )
    memcpy( msg->data, data, (len > MAX_MSG_LEN ? MAX_MSG_LEN : len) );

  *queue->tail = (*queue->tail + 1) % queue->length;

  mutex_unlock(&queue->lock);
  return 0;
}

int post_message( struct Message *queue, mid_t recipient,
                  unsigned subject, mid_t reply, void *data,
                  size_t len )
{
  return -1;
}

int __get_message( struct MessageQueue *queue, struct Message *msg )
{
  if( queue == NULL || queue->pointer == NULL || 
      queue->tail == NULL )
  {
    return -1;
  }
//  else if( mutex_lock(&queue->lock) )
//    return -2;

  while( mutex_lock(&queue->lock) )
    __yield();

  if( *queue->tail == *queue->pointer )
  {
    mutex_unlock(&queue->lock);
    return 1;
  }

  if( msg != NULL )
    *msg = queue->queueAddr[*queue->pointer];

  *queue->pointer = (*queue->pointer + 1) % queue->length; // It's important that this is last

  mutex_unlock(&queue->lock);
  return 0;
}

int _get_message( struct MessageQueue *queue, tid_t *sender, size_t *msg_len,
                  mid_t *recipient, mid_t *reply, void *buffer, 
                  unsigned *protocol, size_t buffer_len )
{
  volatile struct Message *msg;

  if( queue == NULL || queue->pointer == NULL || queue->tail == NULL ||
      recipient == NULL )
  {
    return -1;
  }
//  else if( mutex_lock(&queue->lock) )
//    return -2;

  while( mutex_lock(&queue->lock) )
    __yield();

  if( *queue->tail == *queue->pointer )
  {
    mutex_unlock(&queue->lock);
    return 1;
  }

  msg = &queue->queueAddr[*queue->pointer];

  if( sender != NULL )
    *sender = msg->sender;

  if( recipient != NULL )
    *recipient = msg->recipient;

  if( protocol != NULL )
    *protocol = msg->protocol;

  if( msg_len != NULL )
    *msg_len = msg->length;

  if( reply != NULL )
    *reply = msg->reply_mid;

  if( msg->length > MAX_MSG_LEN )
    print("_get_message(): Message is too long!\n");

  /* XXX: Return an error code if the message is too large for the buffer */

  if( buffer != NULL )
    memcpy( buffer, msg->data, (msg->length > buffer_len ? buffer_len : msg->length)/*(buffer_len > MAX_MSG_LEN ? MAX_MSG_LEN : buffer_len)*/);

  *queue->pointer = (*queue->pointer + 1) % queue->length;

  mutex_unlock(&queue->lock);
  return 0;
}
