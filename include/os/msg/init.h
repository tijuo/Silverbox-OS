#ifndef OS_MSG_INIT_H
#define OS_MSG_INIT_H

#include <util.h>
#include <types.h>

#define INIT_SERVER_TID			1024

#define SERVER_TYPE_DRIVER
#define SERVER_TYPE_FS
#define SERVER_TYPE_PAGER

struct MapRequest
{
  addr_t addr;
  int device;
  size_t length;
  size_t offset;
  int flags;
} __PACKED__;

struct MapReply
{
  addr_t addr;   // the address to the newly mapped memory
} __PACKED__;

struct UnmapRequest
{
  addr_t addr;
  size_t length;
} __PACKED__;

struct CreatePortRequest
{
  pid_t pid;
  int flags;
} __PACKED__;

struct CreatePortReply
{
  pid_t pid;
} __PACKED__;

struct DestroyPortRequest
{
  pid_t port;
} __PACKED__;

struct SendMessageRequest
{
  pid_t port;
  addr_t buffer;
  size_t bufferLen;
} __PACKED__;

struct ReceiveMessageRequest
{
  pid_t port;
  addr_t buffer;    // The buffer set aside for a message (leave NULL to automatically choose)
  size_t bufferLen; // The maximum amount of space for a message
} __PACKED__;

struct RegisterServerRequest
{
  int type;
} __PACKED__;

struct RegisterNameRequest
{
  char name[20];
} __PACKED__;

struct UnregisterNameRequest
{
  char name[20];
} __PACKED__;

struct LookupNameRequest
{
  char name[20];
} __PACKED__;

struct LookupNameReply
{
  tid_t tid;
} __PACKED__;

#endif /* OS_MSG_INIT_H */
