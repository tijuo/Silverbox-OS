#ifndef PORT_H
#define PORT_H

#define PORT_ASYNC		1
#define PORT_SYNC		0
#define PORT_BCAST		2
#define PORT_BIDIR		4
#define PORT_UNIDIR		0
#define PORT_ALLOW		0	// allow access to any thread not in the acl
#define PORT_DENY		8	// deny access to any thread not in the acl by default

struct PortOpRequest
{
  char type; // create, listen, lookup, destroy
}  PACKED;

struct Port
{
  pid_t pid;
  SBAssocArray boundThreads; // tid ->
  SBAssocArray acl; // tid -> send/receive/owner/broadcast
  int flags;
};

#endif /* PORT_H */
