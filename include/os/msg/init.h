#ifndef OS_MSG_INIT_H
#define OS_MSG_INIT_H

#include <util.h>
#include <types.h>

#define INIT_SERVER_TID			256u

#define SERVER_TYPE_DRIVER		1
#define SERVER_TYPE_FS			2
#define SERVER_TYPE_PAGER		3

#define MAX_NAME_LEN			32

struct MapRequest {
    addr_t addr;
    int device;
    uint64_t offset;
    size_t length;
    int flags;
};

struct MapResponse {
    addr_t addr;   // the address to the newly mapped memory
};

struct UnmapRequest {
    addr_t addr;
    size_t length;
};

struct CreatePortRequest {
    pid_t pid;
    int flags;
};

struct CreatePortResponse {
    pid_t pid;
};

struct DestroyPortRequest {
    pid_t port;
};

struct SendMessageRequest {
    pid_t port;
    addr_t buffer;
    size_t bufferLen;
};

struct ReceiveMessageRequest {
    pid_t port;
    addr_t buffer;    // The buffer set aside for a message (leave NULL to automatically choose)
    size_t bufferLen; // The maximum amount of space for a message
};

struct RegisterServerRequest {
    int type;
};

struct RegisterNameRequest {
    size_t name_length;
    char name[];
};

struct UnregisterNameRequest {
    size_t name_length;
    char name[];
};

struct LookupNameRequest {
    size_t name_length;
    char name[];
};

struct LookupNameResponse {
    tid_t tid;
};

#endif /* OS_MSG_INIT_H */
